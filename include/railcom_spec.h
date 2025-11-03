#pragma once

#include <cstdint>

#include "pico/types.h"

// 2012 or 2021
#define RAILCOMSPEC_VERSION 2021

namespace RailComSpec {

constexpr uint baud = 250'000;

// These are only sorta in the spec. The presence of ack, nak, etc., and
// where they are in the decode table is in the spec. The values we use
// here to represent them are arbitrary but they must be 0x40 or higher.
enum DecId : uint8_t {
    dec_max = 0x40,
    dec_ack, // only non-data value seen from LokSound 5
    dec_nak, // never seen from LokSound 5
#if RAILCOMSPEC_VERSION == 2012
    dec_bsy, // doesn't exist in the 2021 draft
#endif
    dec_res,        // encoded value explicitly reserved in spec
    dec_inv = 0xff, // not a valid 4/8 encoding
};

// Lookup table: index this by 8-bit encoded 4/8 code to get 6-bit decoded value
extern const uint8_t decode[UINT8_MAX + 1];

constexpr int ch1_bytes = 2;
constexpr int ch2_bytes = 6;

// Packet IDs (4 bits).
// These are the constants we look for when parsing the railcom data
// (except for pkt_inv which is used to indicate an unset value).
enum PktId : uint8_t {
    pkt_pom = 0,
    pkt_ahi = 1,
    pkt_alo = 2,
    pkt_ext = 3,
    // reserved 4
    // reserved 5
    // reserved 6
    pkt_dyn = 7,
    pkt_xpom = 8,
    // xpom also 9
    // xpom also 10
    // xpom also 11
    pkt_test = 12,
    pkt_block = 13,
    // reserved 14
    // reserved 15
    pkt_inv = 255 // valid values are 4 bits
};

// Dynamic variable IDs (6 bits).
enum DynId : uint8_t {
    dyn_speed_1 = 0,
    dyn_speed_2 = 1,
    // reserved 2
    // reserved 3
    // reserved 4
    dyn_flag = 5,
    dyn_input = 6,
    dyn_stats = 7,
    dyn_cont_1 = 8,
    dyn_cont_2 = 9,
    dyn_cont_3 = 10,
    dyn_cont_4 = 11,
    dyn_cont_5 = 12,
    dyn_cont_6 = 13,
    dyn_cont_7 = 14,
    dyn_cont_8 = 15,
    dyn_cont_9 = 16,
    dyn_cont_10 = 17,
    dyn_cont_11 = 18,
    dyn_cont_12 = 19,
    dyn_address = 20,
    dyn_status = 21,
    dyn_odom = 22,
    dyn_time = 23,
    // reserved 24...63
    dyn_max = 64,
    dyn_inv = 255, // valid values are 6 bits
};

const char *dyn_name(DynId id);

}; // namespace RailComSpec
