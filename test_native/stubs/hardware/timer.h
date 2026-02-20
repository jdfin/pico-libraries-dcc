#pragma once

#include <chrono>
#include <cstdint>

inline uint64_t time_us_64()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<microseconds>(
               steady_clock::now().time_since_epoch())
        .count();
}
