#pragma once

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "pico/types.h"

typedef struct pwm_config {
    uint32_t clkdiv;
    uint32_t wrap;
} pwm_config;

#ifdef __cplusplus

inline uint pwm_gpio_to_slice_num(uint gpio) { return gpio / 2; }
inline uint pwm_gpio_to_channel(uint gpio) { return gpio % 2; }

inline pwm_config pwm_get_default_config() { return {1, 0xffff}; }
inline void pwm_config_set_clkdiv_int(pwm_config *c, uint div) { c->clkdiv = div; }
inline void pwm_init(uint slice, const pwm_config *c, bool start)
{
    (void)slice; (void)c; (void)start;
}
inline void pwm_set_enabled(uint slice, bool en) { (void)slice; (void)en; }
inline void pwm_set_wrap(uint slice, uint16_t wrap) { (void)slice; (void)wrap; }
inline void pwm_set_chan_level(uint slice, uint chan, uint16_t level)
{
    (void)slice; (void)chan; (void)level;
}
inline void pwm_clear_irq(uint slice) { (void)slice; }
inline void pwm_set_irq_enabled(uint slice, bool en) { (void)slice; (void)en; }

#else

static inline unsigned int pwm_gpio_to_slice_num(unsigned int gpio) { return gpio / 2; }
static inline unsigned int pwm_gpio_to_channel(unsigned int gpio) { return gpio % 2; }

#endif
