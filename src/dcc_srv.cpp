#include <ctype.h> // toupper
#include <strings.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
// pico
#include "hardware/sync.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/util/queue.h"
// misc
#include "args.h"
#include "str_ops.h"
#include "sys_led.h"
// dcc
#include "dcc_adc.h"
#include "dcc_bitstream.h"
#include "dcc_command.h"
#include "dcc_cv.h"
#include "dcc_loco.h"
#include "dcc_pkt.h"
#include "dcc_srv.h"
#include "railcom.h"


queue_t req_queue; // requests, core0 -> core1
queue_t rsp_queue; // responses, core1 -> core0
queue_t not_queue; // notifications, core1 -> core0

// Some commands, mainly service-mode reads and writes, take a while (a few
// hundred msec) to complete. When one of these is started, a function pointer
// is set to poll for progress/completion. Each time through the main loop()
// function, the "active" function is called; it is most often the no-op
// function, but is set to some other progress function when needed.

typedef bool(loop_func)();

static bool loop_nop();
static bool loop_svc_cv_get();
static bool loop_svc_cv_set();
static bool loop_svc_address_get();
static bool loop_svc_address_set();

static struct {
    int cv_num; // cv number read/write in progress (1, 17, 18, 29)
    int address;
} loop_svc_address_state;

static loop_func *active = &loop_nop;

// Commands are single letters + arguments.
// Top level (first character):
static inline bool cmd_is_track(char cmd) { return cmd == 'T' || cmd == 't'; }
static inline bool cmd_is_cv(char cmd) { return cmd == 'C' || cmd == 'c'; }
static inline bool cmd_is_address(char cmd) { return cmd == 'A' || cmd == 'a'; }
static inline bool cmd_is_loco(char cmd) { return cmd == 'L' || cmd == 'l'; }
static inline bool cmd_is_debug(char cmd) { return cmd == 'D' || cmd == 'd'; }
// Subcommands (second and later characters):
static inline bool cmd_is_get(char cmd) { return cmd == 'G' || cmd == 'g'; }
static inline bool cmd_is_set(char cmd) { return cmd == 'S' || cmd == 's'; }
static inline bool cmd_is_bit(char cmd) { return cmd == 'B' || cmd == 'b'; }
static inline bool cmd_is_new(char cmd) { return cmd == 'N' || cmd == 'n'; }
static inline bool cmd_is_del(char cmd) { return cmd == 'D' || cmd == 'd'; }
static inline bool cmd_is_func(char cmd) { return cmd == 'F' || cmd == 'f'; }
static inline bool cmd_is_speed(char cmd) { return cmd == 'S' || cmd == 's'; }
static inline bool cmd_is_read(char cmd) { return cmd == 'R' || cmd == 'r'; }

// These functions look at commands and see if there are any valid commands
// to process.

static bool process_msg(const Args &a, char *rsp);
static bool track_msg(const Args &a, char *rsp);
static bool cv_msg(const Args &a, char *rsp);
static bool address_msg(const Args &a, char *rsp);
static bool loco_msg(const Args &a, char *rsp);
static bool debug_msg(const Args &a, char *rsp);

// DCC interface

static DccAdc *adc = nullptr;
static DccCommand *command = nullptr;

// The start time of a long operation (read or write in service mode) is saved
// so the overall time can be printed.

static uint32_t start_us = 0;

static inline uint32_t usec_to_msec(uint32_t us)
{
    return (us + 500) / 1000;
}

// Fill this in before spawning dcc_srv
DccConfig dcc_config;

// spawned on core 1
extern "C" void dcc_srv()
{
    adc = new DccAdc(dcc_config.adc_gpio);

    command = new DccCommand(dcc_config.sig_gpio, dcc_config.pwr_gpio, -1, adc,
                             dcc_config.rcom_uart, dcc_config.rcom_gpio);

    adc->log_reset(); // logging must be enabled by calling adc.log_init()

    while (true) {

        // If any command is ongoing, see if it has made progress
        if (!(*active)())
            active = &loop_nop; // loop_* returning false means it's done

        // Check for new message if we're not in the middle of something
        if (active == &loop_nop) {
            // Get message if available.
            char msg[req_msg_len_max];
            if (queue_try_remove(&req_queue, msg)) {
                // If process_msg returns true, it has filled in a response
                // and we should send it. Otherwise, it has started something
                // that will send a response later.
                // Note that process_msg uses the Args object as input, and
                // reuses the msg buffer for the response (if needed).
                Args a(msg);
                if (process_msg(a, msg))
                    queue_add_blocking(&rsp_queue, msg);
            }
        }

    } // while (true)
}


static bool process_msg(const Args &a, char *rsp)
{
    if (a.argc() < 1 || a[0].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char cmd = a[0].c;
    if (cmd_is_track(cmd)) {
        return track_msg(a, rsp);
    } else if (cmd_is_cv(cmd)) {
        return cv_msg(a, rsp);
    } else if (cmd_is_address(cmd)) {
        return address_msg(a, rsp);
    } else if (cmd_is_loco(cmd)) {
        return loco_msg(a, rsp);
    } else if (cmd_is_debug(cmd)) {
        return debug_msg(a, rsp);
    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

} // process_msg


// track_msg /////////////////////////////////////////////////////////////////
//
// @req "T G" -> "OK 0|1"
// @req "T S 0|1" -> "OK"
//

static bool track_get_msg(const Args &a, char *rsp);
static bool track_set_msg(const Args &a, char *rsp);

static bool track_msg(const Args &a, char *rsp)
{
    // already checked "T ..."
    assert(a.argc() >= 1);
    assert(a[0].t == Args::Type::CHAR && cmd_is_track(a[0].c));

    // a[1] is cmd ('G' or 'S')
    if (a.argc() < 2 || a[1].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char cmd = a[1].c;
    if (cmd_is_get(cmd)) {
        return track_get_msg(a, rsp);
    } else if (cmd_is_set(cmd)) {
        return track_set_msg(a, rsp);
    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

} // track_msg


static bool track_get_msg(const Args &a, char *rsp)
{
    // already checked "T G ..."
    assert(a.argc() >= 2);
    assert(a[0].t == Args::Type::CHAR && cmd_is_track(a[0].c));
    assert(a[1].t == Args::Type::CHAR && cmd_is_get(a[1].c));

    if (a.argc() != 2) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    strcpy(rsp, command->mode() == DccCommand::Mode::OFF ? "OK 0" : "OK 1");
    return true;
}


static bool track_set_msg(const Args &a, char *rsp)
{
    // already checked "T S ..."
    assert(a.argc() >= 2);
    assert(a[0].t == Args::Type::CHAR && cmd_is_track(a[0].c));
    assert(a[1].t == Args::Type::CHAR && cmd_is_set(a[1].c));

    if (a.argc() != 3 || a[2].t != Args::Type::INT) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const int setting = a[2].i;
    if (setting == 0) {
        if (command->mode() == DccCommand::Mode::OPS)
            command->set_mode_off();
        strcpy(rsp, "OK");
        return true;
    } else if (setting == 1) {
        if (command->mode() == DccCommand::Mode::OFF)
            command->set_mode_ops();
        strcpy(rsp, "OK");
        return true;
    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }
}


// cv_msg ////////////////////////////////////////////////////////////////////
//
// @req "C <cv_num> G" -> "OK <cv_val> in <time_ms> ms"
// @req "C <cv_num> S <cv_val>" -> "OK in <time_ms> ms"
// @req "C <cv_num> B <bit_num> G" -> "OK <bit_val> in <time_ms> ms"
// @req "C <cv_num> B <bit_num> S <bit_val>" -> "OK in <time_ms> ms"
//
// @arg cv_num:        1-1024     CV number
// @arg cv_val:        0-255      CV value
// @arg bit_num:       0-7        bit number
// @arg bit_val:       0-1        bit value
//

static bool cv_get_msg(const Args &a, char *rsp);
static bool cv_set_msg(const Args &a, char *rsp);
static bool cv_bit_msg(const Args &a, char *rsp);

static bool cv_msg(const Args &a, char *rsp)
{
    // already checked "C ..."
    assert(a.argc() >= 1);
    assert(a[0].t == Args::Type::CHAR && cmd_is_cv(a[0].c));

    // a[1] is cv_num
    if (a.argc() < 2 || a[1].t != Args::Type::INT || //
        a[1].i < DccPkt::cv_num_min || a[1].i > DccPkt::cv_num_max) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // a[2] is cmd ('G', 'S', or 'B')
    if (a.argc() < 3 || a[2].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // track must be off
    // XXX It might be better if DccCommand returned an error for attempting
    //     service mode operations when track is on, but in the current
    //     implementation there's no return code in a good place.
    if (command->mode() != DccCommand::Mode::OFF) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char cmd = a[2].c;
    if (cmd_is_get(cmd)) {
        return cv_get_msg(a, rsp);
    } else if (cmd_is_set(cmd)) {
        return cv_set_msg(a, rsp);
    } else if (cmd_is_bit(cmd)) {
        return cv_bit_msg(a, rsp);
    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

} // cv_msg


static bool cv_get_msg(const Args &a, char *rsp)
{
    // already checked "C <cv_num> G ..."
    assert(a.argc() >= 3);
    assert(a[0].t == Args::Type::CHAR && cmd_is_cv(a[0].c));
    assert(a[1].t == Args::Type::INT); //cv_num range checked
    assert(a[2].t == Args::Type::CHAR && cmd_is_get(a[2].c));

    if (a.argc() != 3) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const int cv_num = a[1].i;
    command->read_cv(cv_num);

    // we're in service mode for the next couple hundred milliseconds
    start_us = time_us_32();
    active = loop_svc_cv_get;

    return false; // response sent later by loop_svc_cv_get

} // cv_get_msg


static bool cv_set_msg(const Args &a, char *rsp)
{
    // already checked "C <cv_num> S ..."
    assert(a.argc() >= 3);
    assert(a[0].t == Args::Type::CHAR && cmd_is_cv(a[0].c));
    assert(a[1].t == Args::Type::INT); //cv_num range checked
    assert(a[2].t == Args::Type::CHAR && cmd_is_set(a[2].c));

    if (a.argc() != 4 || a[3].t != Args::Type::INT || //
        a[3].i < DccPkt::cv_val_min || a[3].i > DccPkt::cv_val_max) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const int cv_num = a[1].i;
    const int cv_val = a[3].i;
    command->write_cv(cv_num, cv_val);

    // we're in service mode for the next couple hundred milliseconds
    start_us = time_us_32();
    active = loop_svc_cv_set;

    return false; // response sent later by loop_svc_cv_set

} // cv_set_msg


static bool cv_bit_set_msg(const Args &a, char *rsp);
static bool cv_bit_get_msg(const Args &a, char *rsp);

static bool cv_bit_msg(const Args &a, char *rsp)
{
    // already checked "C <cv_num> B ..."
    assert(a.argc() >= 3);
    assert(a[0].t == Args::Type::CHAR && cmd_is_cv(a[0].c));
    assert(a[1].t == Args::Type::INT); // cv_num range checked
    assert(a[2].t == Args::Type::CHAR && cmd_is_bit(a[2].c));

    // a[3] is bit number (0..7)
    if (a.argc() < 4 || a[3].t != Args::Type::INT || //
        a[3].i < 0 || a[3].i > 7) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // a[4] is subcmd ('G' or 'S')
    if (a.argc() < 5 || a[4].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char subcmd = a[4].c;
    if (cmd_is_get(subcmd)) {
        return cv_bit_get_msg(a, rsp);
    } else if (cmd_is_set(subcmd)) {
        return cv_bit_set_msg(a, rsp);
    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

} // cv_bit_msg


static bool cv_bit_get_msg(const Args &a, char *rsp)
{
    // already checked "C <cv_num> B <bit_num> G ..."
    assert(a.argc() >= 5);
    assert(a[0].t == Args::Type::CHAR && cmd_is_cv(a[0].c));
    assert(a[1].t == Args::Type::INT); // cv_num range checked
    assert(a[2].t == Args::Type::CHAR && cmd_is_bit(a[2].c));
    assert(a[3].t == Args::Type::INT); // bit_num range checked
    assert(a[4].t == Args::Type::CHAR && cmd_is_get(a[4].c));

    if (a.argc() != 5) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const int cv_num = a[1].i;
    const int bit_num = a[3].i;
    command->read_bit(cv_num, bit_num);

    // we're in service mode for the next couple hundred milliseconds
    start_us = time_us_32();
    active = loop_svc_cv_get;

    return false; // response sent later by loop_svc_cv_get

} // cv_bit_get_msg


static bool cv_bit_set_msg(const Args &a, char *rsp)
{
    // already checked "C <cv_num> B <bit_num> S ..."
    assert(a.argc() >= 5);
    assert(a[0].t == Args::Type::CHAR && cmd_is_cv(a[0].c));
    assert(a[1].t == Args::Type::INT); // cv_num range checked
    assert(a[2].t == Args::Type::CHAR && cmd_is_bit(a[2].c));
    assert(a[3].t == Args::Type::INT); // bit_num range checked
    assert(a[4].t == Args::Type::CHAR && cmd_is_set(a[4].c));

    // a[5] is bit value (0..1)
    if (a.argc() != 6 || a[5].t != Args::Type::INT || //
        (a[5].i != 0 && a[5].i != 1)) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const int cv_num = a[1].i;
    const int bit_num = a[3].i;
    const int bit_val = a[5].i;
    command->write_bit(cv_num, bit_num, bit_val);

    // we're in service mode for the next couple hundred milliseconds
    start_us = time_us_32();
    active = loop_svc_cv_set;

    return false; // response sent later by loop_svc_cv_set

} // cv_bit_set_msg


// address_msg ///////////////////////////////////////////////////////////////
//
// @req "A G" -> "OK <addr> in <time_ms> ms"
// @req "A S <addr>" -> "OK in <time_ms> ms"
//
// @arg addr:          1-10239    loco address
//

static bool address_get_msg(const Args &a, char *rsp);
static bool address_set_msg(const Args &a, char *rsp);

static bool address_msg(const Args &a, char *rsp)
{
    // already checked "A ..."
    assert(a.argc() >= 1);
    assert(a[0].t == Args::Type::CHAR && cmd_is_address(a[0].c));

    // a[1] is cmd ('G' or 'S')
    if (a.argc() < 2 || a[1].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // track must be off
    // XXX It might be better if DccCommand returned an error for attempting
    //     service mode operations when track is on, but in the current
    //     implementation there's no return code in a good place.
    if (command->mode() != DccCommand::Mode::OFF) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char cmd = a[1].c;
    if (cmd_is_get(cmd)) {
        return address_get_msg(a, rsp);
    } else if (cmd_is_set(cmd)) {
        return address_set_msg(a, rsp);
    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

} // address_msg


static bool address_get_msg(const Args &a, char *rsp)
{
    // already checked "A G ..."
    assert(a.argc() >= 2);
    assert(a[0].t == Args::Type::CHAR && cmd_is_address(a[0].c));
    assert(a[1].t == Args::Type::CHAR && cmd_is_get(a[1].c));

    if (a.argc() != 2) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // start by reading CV29[5] to see if it's long or short
    command->read_bit(DccCv::config, 5);

    // we're in service mode for a while
    start_us = time_us_32();
    loop_svc_address_state.cv_num = DccCv::config; // reading CV29[5]
    active = loop_svc_address_get;

    return false; // loop_svc_address_get will send response when done

} // address_get_msg


static bool address_set_msg(const Args &a, char *rsp)
{
    // already checked "A S ..."
    assert(a.argc() >= 2);
    assert(a[0].t == Args::Type::CHAR && cmd_is_address(a[0].c));
    assert(a[1].t == Args::Type::CHAR && cmd_is_set(a[1].c));

    // a[2] is address
    if (a.argc() != 3 || a[2].t != Args::Type::INT || //
        a[2].i < DccPkt::address_min || a[2].i > DccPkt::address_max) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // Short address is <= 127
    // Short address: write CV1, then clear CV29 bit 5
    // Long address: write CV18 and CV17, then set CV29 bit 5

    const int addr = a[2].i;
    if (addr <= 127) {
        // short address
        command->write_cv(DccCv::address, addr);
        loop_svc_address_state.cv_num = DccCv::address; // writing CV1
    } else {
        // long address
        loop_svc_address_state.address = addr;
        command->write_cv(DccCv::address_hi, (addr >> 8) & 0x3f);
        loop_svc_address_state.cv_num = DccCv::address_hi; // writing CV17
    }

    // we're in service mode for a while
    start_us = time_us_32();
    active = loop_svc_address_set;

    return false; // loop_svc_address_set will send response when done

} // address_set_msg


// loco_msg //////////////////////////////////////////////////////////////////
//
// @req "L <addr> N" -> "OK"
// @req "L <addr> D" -> "OK"
// @req "L <addr> F <f_num> G" -> "OK 0|1"
// @req "L <addr> F <f_num> S 0|1" -> "OK"
// @req "L <addr> S G" -> "OK <speed>"
// @req "L <addr> S S <speed>" -> "OK"
// @req "L <addr> S R 0|1" -> "OK"
// @req "L <addr> C <cv_num> G" -> "OK <cv_val> in <time_ms> ms"
// @req "L <addr> C <cv_num> S <cv_val>" -> "OK <cv_val> in <time_ms> ms"
// @req "L <addr> C <cv_num> B <bit_num> S <bit_val>" -> "OK <cv_val> in <time_ms> ms"
//
// @arg addr:          1-10239    loco address
// @arg f_num:         0-31       function number
// @arg speed:      -127-127      speed value
// @arg cv_num:        1-1024     CV number
// @arg cv_val:        0-255      CV value
// @arg bit_num:       0-7        bit number
// @arg bit_val:       0-1        bit value
//

static bool loco_new_msg(const Args &a, char *rsp, int addr);
static bool loco_del_msg(const Args &a, char *rsp, DccLoco *loco);
static bool loco_func_msg(const Args &a, char *rsp, DccLoco *loco);
static bool loco_speed_msg(const Args &a, char *rsp, DccLoco *loco);
static bool loco_cv_msg(const Args &a, char *rsp, DccLoco *loco);

static bool loco_msg(const Args &a, char *rsp)
{
    // already checked "L ..."
    assert(a.argc() >= 1);
    assert(a[0].t == Args::Type::CHAR && cmd_is_loco(a[0].c));

    // a[1] is address
    if (a.argc() < 2 || a[1].t != Args::Type::INT || //
        a[1].i < DccPkt::address_min || a[1].i > DccPkt::address_max) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // All commands will first look up the loco by address
    // (note the correct value can be nullptr)

    const int addr = a[1].i;
    DccLoco *loco = command->find_loco(addr);

    // a[2] is cmd
    if (a.argc() < 3 || a[2].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char cmd = a[2].c;
    if (cmd_is_new(cmd)) {
        return loco_new_msg(a, rsp, addr);
    } else if (cmd_is_del(cmd)) {
        return loco_del_msg(a, rsp, loco);
    } else if (cmd_is_func(cmd)) {
        return loco_func_msg(a, rsp, loco);
    } else if (cmd_is_speed(cmd)) {
        return loco_speed_msg(a, rsp, loco);
    } else if (cmd_is_cv(cmd)) {
        return loco_cv_msg(a, rsp, loco);
    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

} // loco_msg


static bool loco_new_msg(const Args &a, char *rsp, int addr)
{
    // already checked "L <addr> N ..."
    assert(a.argc() >= 3);
    assert(a[0].t == Args::Type::CHAR && cmd_is_loco(a[0].c));
    assert(a[1].t == Args::Type::INT);
    assert(a[2].t == Args::Type::CHAR && cmd_is_new(a[2].c));

    if (a.argc() != 3) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    if (command->find_loco(addr) != nullptr) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    if (command->create_loco(addr) == nullptr) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    strcpy(rsp, "OK");
    return true;
}


static bool loco_del_msg(const Args &a, char *rsp, DccLoco *loco)
{
    // already checked "L <addr> D ..."
    assert(a.argc() >= 3);
    assert(a[0].t == Args::Type::CHAR && cmd_is_loco(a[0].c));
    assert(a[1].t == Args::Type::INT);
    assert(a[2].t == Args::Type::CHAR && cmd_is_del(a[2].c));

    if (a.argc() != 3) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    if (loco == nullptr) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    command->delete_loco(loco->get_address());
    loco = nullptr;

    strcpy(rsp, "OK");
    return true;
}


static bool loco_func_msg(const Args &a, char *rsp, DccLoco *loco)
{
    // already checked "L <addr> F ..."
    assert(a.argc() >= 3);
    assert(a[0].t == Args::Type::CHAR && cmd_is_loco(a[0].c));
    assert(a[1].t == Args::Type::INT);
    assert(a[2].t == Args::Type::CHAR && cmd_is_func(a[2].c));

    if (loco == nullptr) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // a[3] is fnum
    if (a.argc() < 4 || a[3].t != Args::Type::INT || //
        a[3].i < DccPkt::function_min || a[3].i > DccPkt::function_max) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const int fnum = a[3].i;

    // a[4] is subcmd
    if (a.argc() < 5 || a[4].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char subcmd = a[4].c;

    if (cmd_is_get(subcmd)) {

        if (a.argc() != 5) {
            snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
            return true;
        }

        bool on = loco->get_function(fnum);
        sprintf(rsp, "OK %d", on ? 1 : 0);
        return true;

    } else if (cmd_is_set(subcmd)) {

        if (a.argc() != 6 || a[5].t != Args::Type::INT || //
            (a[5].i != 0 && a[5].i != 1)) {
            snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
            return true;
        }

        const int setting = a[5].i;
        loco->set_function(fnum, setting == 1);
        strcpy(rsp, "OK");
        return true;

    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

} // loco_func_msg


// careful: this is called at interrupt level in the DccBitstream's get_packet
static void loco_speed_cb(DccLoco *loco, uint32_t time_ms, int speed)
{
    char msg[not_msg_len_max];

    snprintf(msg, sizeof(msg), "L %d S %d T %lu", //
             loco->get_address(), speed, time_ms);

    queue_try_add(&not_queue, msg);
}


static bool loco_speed_msg(const Args &a, char *rsp, DccLoco *loco)
{
    // already checked "L <addr> S ..."
    assert(a.argc() >= 3);
    assert(a[0].t == Args::Type::CHAR && cmd_is_loco(a[0].c));
    assert(a[1].t == Args::Type::INT);
    assert(a[2].t == Args::Type::CHAR && cmd_is_speed(a[2].c));

    if (loco == nullptr) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // a[3] is subcmd ('G', 'S', or 'R')
    if (a.argc() < 4 || a[3].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char subcmd = a[3].c;

    if (cmd_is_get(subcmd)) {

        if (a.argc() != 4) {
            snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
            return true;
        }

        sprintf(rsp, "OK %d", loco->get_speed());
        return true;

    } else if (cmd_is_set(subcmd)) {

        if (a.argc() != 5 || a[4].t != Args::Type::INT || //
            a[4].i < DccPkt::speed_min || a[4].i > DccPkt::speed_max) {
            snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
            return true;
        }

        const int speed = a[4].i;
        loco->set_speed(speed);

        strcpy(rsp, "OK");
        return true;

    } else if (cmd_is_read(subcmd)) {

        if (a.argc() != 5 || a[4].t != Args::Type::INT || //
            (a[4].i != 0 && a[4].i != 1)) {
            snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
            return true;
        }

        const int setting = a[4].i;
        if (setting == 1)
            loco->speed_cb_set(loco_speed_cb);
        else
            loco->speed_cb_set(nullptr);

        strcpy(rsp, "OK");
        return true;

    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

} // loco_speed_msg


static bool loco_cv_get_msg(const Args &a, char *rsp, DccLoco *loco);
static bool loco_cv_set_msg(const Args &a, char *rsp, DccLoco *loco);
static bool loco_cv_bit_msg(const Args &a, char *rsp, DccLoco *loco);

static bool loco_cv_msg(const Args &a, char *rsp, DccLoco *loco)
{
    // already checked "L <addr> C ..."
    assert(a.argc() >= 3);
    assert(a[0].t == Args::Type::CHAR && cmd_is_loco(a[0].c));
    assert(a[1].t == Args::Type::INT); // loco address
    assert(a[2].t == Args::Type::CHAR && cmd_is_cv(a[2].c));

    // Can't do any of these if the track is not already on
    if (command->mode() == DccCommand::Mode::OFF) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    if (loco == nullptr) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // a[3] is cv_num
    if (a.argc() < 4 || a[3].t != Args::Type::INT || //
        a[3].i < DccPkt::cv_num_min || a[3].i > DccPkt::cv_num_max) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // a[4] is subcmd ('G', 'S', or 'B')
    if (a.argc() < 5 || a[4].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char subcmd = a[4].c;

    if (cmd_is_get(subcmd)) {
        return loco_cv_get_msg(a, rsp, loco);
    } else if (cmd_is_set(subcmd)) {
        return loco_cv_set_msg(a, rsp, loco);
    } else if (cmd_is_bit(subcmd)) {
        return loco_cv_bit_msg(a, rsp, loco);
    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

} // loco_cv_msg


// careful: this is called at interrupt level in the DccBitstream's get_packet
static void loco_cv_done([[maybe_unused]] DccLoco *loco, bool success, uint8_t cv_val)
{
    char rsp[rsp_msg_len_max];

    const uint32_t op_ms = usec_to_msec(time_us_32() - start_us);

    if (success)
        snprintf(rsp, sizeof(rsp), "OK %u in %lu ms", uint(cv_val), op_ms);
    else
        snprintf(rsp, sizeof(rsp), "ERROR in %lu ms", op_ms);

    queue_add_blocking(&rsp_queue, rsp);
}


static bool loco_cv_get_msg(const Args &a, char *rsp, DccLoco *loco)
{
    // already checked "L <addr> C <cv_num> G ..."
    assert(a.argc() >= 5);
    assert(a[0].t == Args::Type::CHAR && cmd_is_loco(a[0].c));
    assert(a[1].t == Args::Type::INT); // loco address
    assert(a[2].t == Args::Type::CHAR && cmd_is_cv(a[2].c));
    assert(a[3].t == Args::Type::INT); // cv_num (range already checked)
    assert(a[4].t == Args::Type::CHAR && cmd_is_get(a[4].c));
    assert(loco != nullptr);

    if (a.argc() != 5) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const int cv_num = a[3].i;
    loco->read_cv(cv_num, loco_cv_done);

    // it takes ~20 msec to read a CV via railcom

    // start_us is unreliable if multiple locos do this at once
    start_us = time_us_32();

    return false; // response sent later by loco_cv_done

} // loco_cv_get_msg


static bool loco_cv_set_msg(const Args &a, char *rsp, DccLoco *loco)
{
    // already checked "L <addr> C <cv_num> S ..."
    assert(a.argc() >= 5);
    assert(a[0].t == Args::Type::CHAR && cmd_is_loco(a[0].c));
    assert(a[1].t == Args::Type::INT); // loco address
    assert(a[2].t == Args::Type::CHAR && cmd_is_cv(a[2].c));
    assert(a[3].t == Args::Type::INT); // cv_num (range already checked)
    assert(a[4].t == Args::Type::CHAR && cmd_is_set(a[4].c));
    assert(loco != nullptr);

    if (a.argc() != 6 || a[5].t != Args::Type::INT) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const int cv_num = a[3].i;
    const int cv_val = a[5].i;
    loco->write_cv(cv_num, cv_val, loco_cv_done);

    // it takes ~20 msec to write a CV via railcom

    // start_us is unreliable if multiple locos do this at once
    start_us = time_us_32();

    return false; // response sent later by loco_cv_done

} // loco_cv_set_msg


// Note: there is no "bit get" in ops mode; you just read the whole CV
static bool loco_cv_bit_msg(const Args &a, char *rsp, DccLoco *loco)
{
    // already checked "L <addr> C <cv_num> B ..."
    assert(a.argc() >= 5);
    assert(a[0].t == Args::Type::CHAR && cmd_is_loco(a[0].c));
    assert(a[1].t == Args::Type::INT); // loco address
    assert(a[2].t == Args::Type::CHAR && cmd_is_cv(a[2].c));
    assert(a[3].t == Args::Type::INT); // cv_num
    assert(a[4].t == Args::Type::CHAR && cmd_is_bit(a[4].c));
    assert(loco != nullptr);

    // a[5] is bit number (0..7)
    if (a.argc() < 6 || a[5].t != Args::Type::INT || //
        a[5].i < 0 || a[5].i > 7) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // a[6] is subcmd ('S' or 's')
    if (a.argc() < 7 || a[6].t != Args::Type::CHAR || !cmd_is_set(a[6].c)) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    // a[7] is bit_val (0 or 1)
    if (a.argc() != 8 || a[7].t != Args::Type::INT || //
        (a[7].i != 0 && a[7].i != 1)) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const int cv_num = a[3].i;
    const int bit_num = a[5].i;
    const int bit_val = a[7].i;
    loco->write_bit(cv_num, bit_num, bit_val, loco_cv_done);

    // it takes ~20 msec to write a CV bit via railcom

    // start_us is unreliable if multiple locos do this at once
    start_us = time_us_32();

    return false; // response sent later by loco_cv_done

} // loco_cv_bit_msg


///// debug functions ////////////////////////////////////////////////////////


static bool debug_msg(const Args &a, char *rsp)
{
    // already checked "D ..."
    assert(a.argc() >= 1);
    assert(a[0].t == Args::Type::CHAR && cmd_is_debug(a[0].c));

    // a[1] is <code>
    if (a.argc() < 2 || a[1].t != Args::Type::INT) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    int code = a[1].i;

    // a[2] is subcmd
    if (a.argc() < 3 || a[2].t != Args::Type::CHAR) {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        return true;
    }

    const char subcmd = a[2].c;

    if (cmd_is_set(subcmd)) {
        // a[3] is <val>
        if (a.argc() != 4 || a[3].t != Args::Type::INT) {
            snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        } else {
            if (code == 0) {
                command->show_dcc(a[3].i != 0);
                strcpy(rsp, "OK");
            } else if (code == 1) {
                command->show_railcom(a[3].i != 0);
                strcpy(rsp, "OK");
            } else {
                snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
            }
        }
    } else if (cmd_is_get(subcmd)) {
        if (a.argc() != 3) {
            snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        } else if (code == 0) {
            snprintf(rsp, rsp_msg_len_max, "OK %d", command->show_dcc());
        } else if (code == 1) {
            snprintf(rsp, rsp_msg_len_max, "OK %d", command->show_railcom());
        } else {
            snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
        }
    } else {
        snprintf(rsp, rsp_msg_len_max, "ERROR %d", __LINE__);
    }
    return true;

} // debug_msg


///// loop functions /////////////////////////////////////////////////////////


// The "active" function when nothing needs doing
static bool loop_nop()
{
    return true;
}


static bool loop_svc_cv_get()
{
    bool result;
    uint8_t value;
    if (!command->svc_done(result, value))
        return true; // keep going

    uint32_t op_ms = usec_to_msec(time_us_32() - start_us);

    char msg[rsp_msg_len_max];

    if (result)
        snprintf(msg, sizeof(msg), "OK %u in %lu ms", uint(value), op_ms);
    else
        snprintf(msg, sizeof(msg), "ERROR in %lu ms", op_ms);

    queue_add_blocking(&rsp_queue, msg);

    return false; // done!
}


static bool loop_svc_cv_set()
{
    bool result;
    if (!command->svc_done(result))
        return true; // keep going

    uint32_t op_ms = usec_to_msec(time_us_64() - start_us);

    char msg[rsp_msg_len_max];

    if (result) {
        snprintf(msg, sizeof(msg), "OK in %lu ms", op_ms);
    } else {
        snprintf(msg, sizeof(msg), "ERROR in %lu ms", op_ms);
    }

    queue_add_blocking(&rsp_queue, msg);

    return false; // done!
}


// sequencing through steps to read address
static bool loop_svc_address_get()
{
    bool result;
    uint8_t value;
    if (!command->svc_done(result, value))
        return true; // keep going

    uint32_t op_ms = usec_to_msec(time_us_64() - start_us); // so far

    char rsp[rsp_msg_len_max];

    if (!result) {
        snprintf(rsp, sizeof(rsp), "ERROR in %lu ms", op_ms);
        queue_add_blocking(&rsp_queue, rsp);
        return false; // done!
    }

    if (loop_svc_address_state.cv_num == DccCv::config) {
        // reading CV29[5]
        if (value == 0) {
            // short address, read CV1 (address)
            command->read_cv(DccCv::address);
            loop_svc_address_state.cv_num = DccCv::address;
        } else {
            // long address, read CV17 (then CV18)
            command->read_cv(DccCv::address_hi);
            loop_svc_address_state.cv_num = DccCv::address_hi;
        }
        return true; // keep going

    } else if (loop_svc_address_state.cv_num == DccCv::address) {
        // reading CV1 (value is address)
        snprintf(rsp, sizeof(rsp), "OK %u in %lu ms", uint(value), op_ms);
        queue_add_blocking(&rsp_queue, rsp);
        return false; // done!

    } else if (loop_svc_address_state.cv_num == DccCv::address_hi) {
        // reading CV17 (value is address_hi); save upper part and read CV18 next
        loop_svc_address_state.address = value & 0x3f;
        command->read_cv(DccCv::address_lo);
        loop_svc_address_state.cv_num = DccCv::address_lo;
        return true; // keep going

    } else if (loop_svc_address_state.cv_num == DccCv::address_lo) {
        // reading CV18 (value is address_lo)
        int address = (loop_svc_address_state.address << 8) | value;
        snprintf(rsp, sizeof(rsp), "OK %u in %lu ms", address, op_ms);
        queue_add_blocking(&rsp_queue, rsp);
        loop_svc_address_state.cv_num = DccCv::invalid;
        return false; // done!
    }

    assert(false);

} // loop_svc_address_get


// sequencing through steps to write address
static bool loop_svc_address_set()
{
    bool result;
    uint8_t value;
    if (!command->svc_done(result, value))
        return true; // keep going

    uint32_t op_ms = usec_to_msec(time_us_64() - start_us); // so far

    char rsp[rsp_msg_len_max];

    if (!result) {
        snprintf(rsp, sizeof(rsp), "ERROR in %lu ms", op_ms);
        queue_add_blocking(&rsp_queue, rsp);
        return false; // done!
    }

    if (loop_svc_address_state.cv_num == DccCv::address) {
        // wrote CV1 (address), clear CV29[5]
        command->write_bit(DccCv::config, 5, 0);
        loop_svc_address_state.cv_num = DccCv::config;
        return true; // keep going

    } else if (loop_svc_address_state.cv_num == DccCv::address_hi) {
        // wrote CV17 (address_hi); write CV18 (address_lo)
        command->write_cv(DccCv::address_lo,
                          loop_svc_address_state.address & 0xff);
        loop_svc_address_state.cv_num = DccCv::address_lo;
        return true; // keep going

    } else if (loop_svc_address_state.cv_num == DccCv::address_lo) {
        // wrote CV18 (address_lo), set CV29[5]
        command->write_bit(DccCv::config, 5, 1);
        loop_svc_address_state.cv_num = DccCv::config;
        return true; // keep going

    } else if (loop_svc_address_state.cv_num == DccCv::config) {
        // wrote CV29[5], done
        snprintf(rsp, sizeof(rsp), "OK in %lu ms", op_ms);
        queue_add_blocking(&rsp_queue, rsp);
        loop_svc_address_state.cv_num = DccCv::invalid;
        return false; // done!
    }

    assert(false);

} // loop_svc_address_set
