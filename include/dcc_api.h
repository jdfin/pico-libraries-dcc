#pragma once

#include <cstdint>

#include "dcc_srv.h"
#include "hardware/uart.h"

namespace DccApi {

enum class Status {
    Ok = 0,
    Timeout,
    Error,
};

const char *status(Status s);

void init(int sig_gpio, int pwr_gpio, int adc_gpio, int rcom_gpio,
          uart_inst_t *rcom_uart);

void loop();

typedef bool(NotifyFunc)(intptr_t arg, const char *msg);

void notify(NotifyFunc *func, intptr_t arg = 0);

constexpr int32_t forever_us = INT32_MAX;

// raw send/receive (for testing)

Status raw_req(const char *req_msg, int32_t timeout_us = 0);
Status raw_rsp(char *rsp_msg, int rsp_max, int32_t timeout_us = 0);
Status raw_not(char *not_msg, int not_max, int32_t timeout_us = 0);

// track power

constexpr int32_t track_timeout_us = 100'000;

Status track_get_start(int32_t end_us);
Status track_get_check(bool &on, int32_t end_us);
Status track_get(bool &on, int32_t timeout_us = track_timeout_us);

Status track_set_start(bool on, int32_t end_us);
Status track_set_check(int32_t end_us);
Status track_set(bool on, int32_t timeout_us = track_timeout_us);

// service mode, cv access

constexpr int32_t cv_get_timeout_us = 2'000'000;
constexpr int32_t cv_set_timeout_us = 2'000'000;

Status cv_val_get_start(int cv_num, int32_t end_us);
Status cv_val_get_check(int &cv_val, int32_t end_us = cv_get_timeout_us);
Status cv_val_get(int cv_num, int &cv_val, int32_t timeout_us = cv_get_timeout_us);

Status cv_val_set_start(int cv_num, int cv_val, int32_t end_us);
Status cv_val_set_check(int32_t end_us);
Status cv_val_set(int cv_num, int cv_val, int32_t timeout_us = cv_set_timeout_us);

Status cv_bit_get_start(int cv_num, int b_num, int32_t end_us);
Status cv_bit_get_check(int &b_val, int32_t end_us);
Status cv_bit_get(int cv_num, int b_num, int &b_val, int32_t timeout_us = cv_get_timeout_us);

Status cv_bit_set_start(int cv_num, int b_num, int b_val, int32_t end_us);
Status cv_bit_set_check(int32_t end_us);
Status cv_bit_set(int cv_num, int b_num, int b_val, int32_t timeout_us = cv_set_timeout_us);

// service mode, loco address

constexpr int32_t addr_get_timeout_us = 5'000'000;
constexpr int32_t addr_set_timeout_us = 5'000'000;

Status addr_get_start(int32_t end_us);
Status addr_get_check(int &addr, int32_t end_us);
Status addr_get(int &addr, int32_t timeout_us = addr_get_timeout_us);

Status addr_set_start(int addr, int32_t end_us);
Status addr_set_check(int32_t end_us);
Status addr_set(int addr, int32_t timeout_us = addr_set_timeout_us);

// loco control

// timeout for operations that do not interact with loco on track
constexpr int32_t loco_op_timeout_us = 100'000;

Status loco_create_start(int addr, int32_t end_us);
Status loco_create_check(int32_t end_us);
Status loco_create(int addr, int32_t timeout_us = loco_op_timeout_us);

Status loco_delete_start(int addr, int32_t end_us);
Status loco_delete_check(int32_t end_us);
Status loco_delete(int addr, int32_t timeout_us = loco_op_timeout_us);

Status loco_func_get_start(int addr, int func, int32_t end_us);
Status loco_func_get_check(bool &on, int32_t end_us);
Status loco_func_get(int addr, int func, bool &on, int32_t timeout_us = loco_op_timeout_us);

Status loco_func_set_start(int addr, int func, bool on, int32_t end_us);
Status loco_func_set_check(int32_t end_us);
Status loco_func_set(int addr, int func, bool on, int32_t timeout_us = loco_op_timeout_us);

Status loco_speed_get_start(int addr, int32_t end_us);
Status loco_speed_get_check(int &speed, int32_t end_us);
Status loco_speed_get(int addr, int &speed, int32_t timeout_us = loco_op_timeout_us);

Status loco_speed_set_start(int addr, int speed, int32_t end_us);
Status loco_speed_set_check(int32_t end_us);
Status loco_speed_set(int addr, int speed, int32_t timeout_us = loco_op_timeout_us);

// XXX speed change notify

// operations that require a railcom response from loco on track
constexpr int32_t loco_cv_op_timeout_us = 1'000'000;

Status loco_cv_val_get_start(int addr, int cv_num, int32_t end_us);
Status loco_cv_val_get_check(int &cv_val, int32_t end_us);
Status loco_cv_val_get(int addr, int cv_num, int &cv_val, int32_t timeout_us = loco_cv_op_timeout_us);

Status loco_cv_val_set_start(int addr, int cv_num, int cv_val, int32_t end_us);
Status loco_cv_val_set_check(int32_t end_us);
Status loco_cv_val_set(int addr, int cv_num, int cv_val, int32_t timeout_us = loco_cv_op_timeout_us);

Status loco_cv_bit_set_start(int addr, int cv_num, int b_num, int b_val, int32_t end_us);
Status loco_cv_bit_set_check(int32_t end_us);
Status loco_cv_bit_set(int addr, int cv_num, int b_num, int b_val, int32_t timeout_us = loco_cv_op_timeout_us);

} // namespace DccApi
