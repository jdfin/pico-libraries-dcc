#pragma once

#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

// max bytes per message
constexpr int req_msg_len_max = 32; // request and response messages
constexpr int rsp_msg_len_max = req_msg_len_max;
constexpr int not_msg_len_max = 32; // notification messages

// max messages per queue
constexpr int req_msg_cnt_max = 8;  // request and response queues
constexpr int rsp_msg_cnt_max = req_msg_cnt_max;
constexpr int not_msg_cnt_max = 32; // notification queue

extern queue_t req_queue; // requests, core0 -> core1
extern queue_t rsp_queue; // responses, core1 -> core0
extern queue_t not_queue; // notifications, core1 -> core0

// Fill in config before spawning dcc_srv.
struct DccConfig {
    int sig_gpio;
    int pwr_gpio;
    int adc_gpio;
    int rcom_gpio;
    uart_inst_t *rcom_uart;
};

extern DccConfig dcc_config;

extern "C" void dcc_srv();
