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

// S 9.2.3 - packet counts for service mode, direct mode
// It's not very clear what the real requirements are

// Reset packets sent after powering up track
constexpr int svc_reset1_cnt = 20;

// Command packets sent
constexpr int svc_command_cnt = 5;

// Reset packets sent after a service mode request
constexpr int svc_reset2_cnt = 5;

}; // namespace DccSpec
