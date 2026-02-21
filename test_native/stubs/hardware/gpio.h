#pragma once

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "pico/types.h"

#define GPIO_OUT 1
#define GPIO_FUNC_PWM 4

// UART_FUNCSEL_NUM is used by railcom.cpp - just return a dummy value
#define UART_FUNCSEL_NUM(uart, gpio) 2

#ifdef __cplusplus
inline void gpio_init(uint gpio) { (void)gpio; }
inline void gpio_put(uint gpio, bool value) { (void)gpio; (void)value; }
inline void gpio_set_dir(uint gpio, bool out) { (void)gpio; (void)out; }
inline void gpio_set_function(uint gpio, uint fn) { (void)gpio; (void)fn; }
#else
static inline void gpio_init(unsigned int gpio) { (void)gpio; }
static inline void gpio_put(unsigned int gpio, int value) { (void)gpio; (void)value; }
static inline void gpio_set_dir(unsigned int gpio, int out) { (void)gpio; (void)out; }
static inline void gpio_set_function(unsigned int gpio, unsigned int fn) { (void)gpio; (void)fn; }
#endif
