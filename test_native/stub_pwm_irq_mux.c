// Stub pwm_irq_mux for native tests

#include <stdint.h>

#include "pwm_x.h"

void pwmx_irqn_set_slice_handler(uint irqn, uint slice, //
                                 void (*func)(intptr_t), intptr_t arg)
{
    (void)irqn;
    (void)slice;
    (void)func;
    (void)arg;
}
