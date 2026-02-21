#pragma once

#include <cstdint>

enum clock_index {
    clk_sys = 0,
};

inline uint32_t clock_get_hz(enum clock_index clk_index)
{
    (void)clk_index;
    return 150000000; // 150 MHz
}
