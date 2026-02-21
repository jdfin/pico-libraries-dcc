#pragma once

#include <cstdint>
#include "pico/types.h"

typedef struct uart_inst uart_inst_t;

inline uint uart_init(uart_inst_t *uart, uint baudrate)
{
    (void)uart; (void)baudrate;
    return baudrate;
}

inline void uart_deinit(uart_inst_t *uart) { (void)uart; }

inline bool uart_is_readable(uart_inst_t *uart)
{
    (void)uart;
    return false;
}

inline uint8_t uart_getc(uart_inst_t *uart)
{
    (void)uart;
    return 0;
}
