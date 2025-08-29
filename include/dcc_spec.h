#pragma once

namespace DccSpec {

// Used when transmitting bits

constexpr int t1_min_us = 55;
constexpr int t1_nom_us = 58;
constexpr int t1_max_us = 61;

constexpr int t1d_max_us = 3;

constexpr int t0_min_us = 95;
constexpr int t0_nom_us = 100;
constexpr int t0_max_us = 9900;

// Used when receiving bits

constexpr int tr1_min_us = 52;
constexpr int tr1_nom_us = 58;
constexpr int tr1_max_us = 64;

constexpr int tr1d_max_us = 6;

constexpr int tr0_min_us = 90;
constexpr int tr0_nom_us = 100;
constexpr int tr0_max_us = 10000;

}; // namespace DccSpec