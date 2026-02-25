// Stub pwm_irq_mux for native tests

#include "pwm_x.h"

void pwm_irq_mux_connect(uint slice_num, void (*func)(void*), void *arg)
{
    (void)slice_num;
    (void)func;
    (void)arg;
}
