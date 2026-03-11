
#include <cstdint>
#include <cstdio>
#include <cstring>
// pico
#include "hardware/sync.h" // __dmb()
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
// misc
#include "str_ops.h" // strxcpy()
// dcc
#include "dcc_api.h"
#include "dcc_srv.h"

// This runs on core 0, and is responsible for creating the inter-core message
// channel and starting core 1 (running dcc_srv). From then on, methods here
// allow an application to communicate with dcc_srv.

// Timeouts are expressed as int32_t microseconds, so the maximum timeout is
// about 35 minutes. There is no way to wait forever. Signed is used to make
// timeouts simpler, and 35 minutes seems like it should be long enough to
// approximate "forever".
//
// Since a timeout covers waiting for the send and the receive, we handle it
// by calculating and "end time" at the start, then timing out if the end time
// is reached. We check to see if the end time has been reached by subtracting
// it from the current time, and timing out if the result is positive. This
// handles any wraparounds on either the current time or the end time for any
// timeouts expressed as int32_t.

int verbosity = 2;

namespace DccApi {


const char *status(Status s)
{
    switch (s) {
    case Status::Ok:
        return "Ok";
    case Status::Timeout:
        return "Timeout";
    case Status::Error:
        return "Error";
    default:
        return "Unknown";
    }
}


// Send request (with timeout)
//
// This is intended to be internal; the following are handled by caller:
// * req_msg must be at least req_msg_len_max characters
// * end_us is time_us_32() + timeout_us (wrap is okay)
//
// Takes ~15 usec if there's room in the queue. The only way queue_try_add
// fails is if the queue is full, which usually means something has gone
// wrong.
//
// Returns
// @ Status::Ok
// @ Status::Timeout
static inline Status req_send(const char *req_msg, int32_t end_us)
{
    while (!queue_try_add(&req_queue, req_msg))
        if ((int32_t(time_us_32()) - end_us) >= 0)
            return Status::Timeout;
    return Status::Ok;
}


// Receive response (with timeout)
//
// This is intended to be internal; the following are handled by caller:
// * rsp_msg must be at least rsp_msg_len_max characters
// * end_us is time_us_32() + timeout_us (wrap is okay)
//
// queue_try_remove returns true if there's something in the queue, or false
// if the queue is empty.
//
// Returns
// @ Status::Ok
// @ Status::Timeout
static inline Status rsp_recv(char *rsp_msg, int32_t end_us)
{
    while (!queue_try_remove(&rsp_queue, rsp_msg))
        if ((int32_t(time_us_32()) - end_us) >= 0)
            return Status::Timeout;
    return Status::Ok;
}


// Same as rsp_recv, but for not_queue and not_msg_len_max
static inline Status not_recv(char *not_msg, int32_t end_us)
{
    while (!queue_try_remove(&not_queue, not_msg))
        if ((int32_t(time_us_32()) - end_us) >= 0)
            return Status::Timeout;
    return Status::Ok;
}


void init(int sig_gpio, int pwr_gpio, int adc_gpio, //
          int rcom_gpio, uart_inst_t *rcom_uart)
{
    // create message queues
    // void queue_init (queue_t *q, uint element_size, uint element_count)
    queue_init(&req_queue, req_msg_len_max, req_msg_cnt_max);
    queue_init(&rsp_queue, req_msg_len_max, req_msg_cnt_max);
    queue_init(&not_queue, not_msg_len_max, not_msg_cnt_max);

    dcc_config.sig_gpio = sig_gpio;
    dcc_config.pwr_gpio = pwr_gpio;
    dcc_config.adc_gpio = adc_gpio;
    dcc_config.rcom_gpio = rcom_gpio;
    dcc_config.rcom_uart = rcom_uart;

    __dmb(); // make sure config is visible in RAM before starting core 1

    // start core 1
    multicore_launch_core1(dcc_srv);
}


static constexpr int notify_func_max = 8;

static struct {
    NotifyFunc *func;
    intptr_t arg;
} notify_func[notify_func_max];

static int notify_func_cnt = 0;


void notify(NotifyFunc *func, intptr_t arg)
{
    assert(notify_func_cnt < notify_func_max);
    notify_func[notify_func_cnt].func = func;
    notify_func[notify_func_cnt].arg = arg;
    notify_func_cnt++;
}


void loop()
{
    char msg[not_msg_len_max];
    if (queue_try_remove(&not_queue, msg)) {
        // call notify functions until one returns true
        for (int i = 0; i < notify_func_cnt; i++) {
            if (notify_func[i].func(notify_func[i].arg, msg))
                break;
        }
    }
}


// raw ////////////////////////////////////////////////////////////////////////

// These send/receive raw messages, with the only help being they do so to a
// local buffer of sufficient size (req_msg_len_max etc.) and calculate end_us
// from timeout_us.

Status raw_req(const char *req_msg, int32_t timeout_us)
{
    char msg[req_msg_len_max];
    strxcpy(msg, req_msg, req_msg_len_max); // msg is always terminated
    return req_send(msg, time_us_32() + timeout_us);
}


Status raw_rsp(char *rsp_msg, int rsp_max, int32_t timeout_us)
{
    char msg[rsp_msg_len_max];
    Status s = rsp_recv(msg, timeout_us);
    if (s == Status::Ok)
        strxcpy(rsp_msg, msg, rsp_max); // rsp_msg is always terminated
    return s;
}


Status raw_not(char *not_msg, int not_max, int32_t timeout_us)
{
    char msg[not_msg_len_max];
    Status s = not_recv(msg, timeout_us);
    if (s == Status::Ok)
        strxcpy(not_msg, msg, not_max); // not_msg is always terminated
    return s;
}


// track_get //////////////////////////////////////////////////////////////////


Status track_get_start(int32_t end_us)
{
    const char req_msg[req_msg_len_max] = "T G";
    return req_send(req_msg, end_us);
}


Status track_get_check(bool &on, int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    if (strcmp(rsp_msg, "OK 0") == 0) {
        on = false;
        return Status::Ok;
    } else if (strcmp(rsp_msg, "OK 1") == 0) {
        on = true;
        return Status::Ok;
    } else {
        return Status::Error;
    }
}


Status track_get(bool &on, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = track_get_start(end_us);
    if (s != Status::Ok)
        return s;
    return track_get_check(on, end_us);
}


// track_set //////////////////////////////////////////////////////////////////


Status track_set_start(bool on, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "T S %d", on);
    return req_send(req_msg, end_us);
}


Status track_set_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status track_set(bool on, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = track_set_start(on, end_us);
    if (s != Status::Ok)
        return s;
    return track_set_check(end_us);
}


// cv_val_get /////////////////////////////////////////////////////////////////


Status cv_val_get_start(int cv_num, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "C %d G", cv_num);
    return req_send(req_msg, end_us);
}


Status cv_val_get_check(int &cv_val, int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return sscanf(rsp_msg, "OK %d", &cv_val) == 1 ? Status::Ok : Status::Error;
}


Status cv_val_get(int cv_num, int &cv_val, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = cv_val_get_start(cv_num, end_us);
    if (s != Status::Ok)
        return s;
    return cv_val_get_check(cv_val, end_us);
}


// cv_val_set /////////////////////////////////////////////////////////////////


Status cv_val_set_start(int cv_num, int cv_val, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "C %d S %d", cv_num, cv_val);
    return req_send(req_msg, end_us);
}


Status cv_val_set_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status cv_val_set(int cv_num, int cv_val, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = cv_val_set_start(cv_num, cv_val, end_us);
    if (s != Status::Ok)
        return s;
    return cv_val_set_check(end_us);
}


// cv_bit_get /////////////////////////////////////////////////////////////////


Status cv_bit_get_start(int cv_num, int b_num, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "C %d B %d G", cv_num, b_num);
    return req_send(req_msg, end_us);
}


Status cv_bit_get_check(int &b_val, int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return sscanf(rsp_msg, "OK %d", &b_val) == 1 ? Status::Ok : Status::Error;
}


Status cv_bit_get(int cv_num, int b_num, int &b_val, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = cv_bit_get_start(cv_num, b_num, end_us);
    if (s != Status::Ok)
        return s;
    return cv_bit_get_check(b_val, end_us);
}


// cv_bit_set /////////////////////////////////////////////////////////////////


Status cv_bit_set_start(int cv_num, int b_num, int b_val, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "C %d B %d S %d", cv_num, b_num, b_val);
    return req_send(req_msg, end_us);
}


Status cv_bit_set_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status cv_bit_set(int cv_num, int b_num, int b_val, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = cv_bit_set_start(cv_num, b_num, b_val, end_us);
    if (s != Status::Ok)
        return s;
    return cv_bit_set_check(end_us);
}


// addr_get ///////////////////////////////////////////////////////////////////


Status addr_get_start(int32_t end_us)
{
    const char req_msg[req_msg_len_max] = "A G";
    return req_send(req_msg, end_us);
}


Status addr_get_check(int &addr, int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return sscanf(rsp_msg, "OK %d", &addr) == 1 ? Status::Ok : Status::Error;
}


Status addr_get(int &addr, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = addr_get_start(end_us);
    if (s != Status::Ok)
        return s;
    return addr_get_check(addr, end_us);
}


// addr_set ///////////////////////////////////////////////////////////////////


Status addr_set_start(int addr, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "A S %d", addr);
    return req_send(req_msg, end_us);
}


Status addr_set_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status addr_set(int addr, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = addr_set_start(addr, end_us);
    if (s != Status::Ok)
        return s;
    return addr_set_check(end_us);
}


// loco_create ////////////////////////////////////////////////////////////////


Status loco_create_start(int addr, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "L %d N", addr);
    return req_send(req_msg, end_us);
}


Status loco_create_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status loco_create(int addr, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = loco_create_start(addr, end_us);
    if (s != Status::Ok)
        return s;
    return loco_create_check(end_us);
}


// loco_delete ////////////////////////////////////////////////////////////////


Status loco_delete_start(int addr, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "L %d D", addr);
    return req_send(req_msg, end_us);
}


Status loco_delete_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status loco_delete(int addr, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = loco_delete_start(addr, end_us);
    if (s != Status::Ok)
        return s;
    return loco_delete_check(end_us);
}


// loco_func_get //////////////////////////////////////////////////////////////


Status loco_func_get_start(int addr, int func, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "L %d F %d G", addr, func);
    return req_send(req_msg, end_us);
}


Status loco_func_get_check(bool &on, int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;

    if (strcmp(rsp_msg, "OK 1") == 0) {
        on = true;
        return Status::Ok;
    } else if (strcmp(rsp_msg, "OK 0") == 0) {
        on = false;
        return Status::Ok;
    } else {
        return Status::Error;
    }
}


Status loco_func_get(int addr, int func, bool &on, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = loco_func_get_start(addr, func, end_us);
    if (s != Status::Ok)
        return s;
    return loco_func_get_check(on, end_us);
}


// loco_func_set //////////////////////////////////////////////////////////////


Status loco_func_set_start(int addr, int func, bool on, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "L %d F %d S %d", addr, func, on);
    return req_send(req_msg, end_us);
}


Status loco_func_set_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status loco_func_set(int addr, int func, bool on, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = loco_func_set_start(addr, func, on, end_us);
    if (s != Status::Ok)
        return s;
    return loco_func_set_check(end_us);
}


// loco_speed_get /////////////////////////////////////////////////////////////


Status loco_speed_get_start(int addr, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "L %d S G", addr);
    return req_send(req_msg, end_us);
}


Status loco_speed_get_check(int &speed, int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return sscanf(rsp_msg, "OK %d", &speed) == 1 ? Status::Ok : Status::Error;
}


Status loco_speed_get(int addr, int &speed, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = loco_speed_get_start(addr, end_us);
    if (s != Status::Ok)
        return s;
    return loco_speed_get_check(speed, end_us);
}


// loco_speed_set /////////////////////////////////////////////////////////////


Status loco_speed_set_start(int addr, int speed, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "L %d S S %d", addr, speed);
    return req_send(req_msg, end_us);
}


Status loco_speed_set_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status loco_speed_set(int addr, int speed, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = loco_speed_set_start(addr, speed, end_us);
    if (s != Status::Ok)
        return s;
    return loco_speed_set_check(end_us);
}


// loco_cv_val_get ////////////////////////////////////////////////////////////


Status loco_cv_val_get_start(int addr, int cv_num, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "L %d C %d G", addr, cv_num);
    return req_send(req_msg, end_us);
}


Status loco_cv_val_get_check(int &cv_val, int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return sscanf(rsp_msg, "OK %d", &cv_val) == 1 ? Status::Ok : Status::Error;
}


Status loco_cv_val_get(int addr, int cv_num, int &cv_val, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = loco_cv_val_get_start(addr, cv_num, end_us);
    if (s != Status::Ok)
        return s;
    return loco_cv_val_get_check(cv_val, end_us);
}


// loco_cv_val_set ////////////////////////////////////////////////////////////


Status loco_cv_val_set_start(int addr, int cv_num, int cv_val, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "L %d C %d S %d", addr, cv_num, cv_val);
    return req_send(req_msg, end_us);
}


Status loco_cv_val_set_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status loco_cv_val_set(int addr, int cv_num, int cv_val, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = loco_cv_val_set_start(addr, cv_num, cv_val, end_us);
    if (s != Status::Ok)
        return s;
    return loco_cv_val_set_check(end_us);
}


// loco_cv_bit_set ////////////////////////////////////////////////////////////


Status loco_cv_bit_set_start(int addr, int cv_num, int b_num, int b_val, int32_t end_us)
{
    char req_msg[req_msg_len_max];
    snprintf(req_msg, req_msg_len_max, "L %d C %d B %d S %d", addr, cv_num,
             b_num, b_val);
    return req_send(req_msg, end_us);
}


Status loco_cv_bit_set_check(int32_t end_us)
{
    char rsp_msg[rsp_msg_len_max];
    Status s = rsp_recv(rsp_msg, end_us);
    if (s != Status::Ok)
        return s;
    return strncmp(rsp_msg, "OK", 2) == 0 ? Status::Ok : Status::Error;
}


Status loco_cv_bit_set(int addr, int cv_num, int b_num, int b_val, int32_t timeout_us)
{
    int32_t end_us = time_us_32() + timeout_us;
    Status s = loco_cv_bit_set_start(addr, cv_num, b_num, b_val, end_us);
    if (s != Status::Ok)
        return s;
    return loco_cv_bit_set_check(end_us);
}


} // namespace DccApi
