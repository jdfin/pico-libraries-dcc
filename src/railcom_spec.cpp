#include "railcom_spec.h"

#include <cassert>
#include <cstdint>


namespace RailComSpec {

#if (RAILCOMSPEC_VERSION != 2012) && (RAILCOMSPEC_VERSION != 2021)
#error RAILCOMSPEC_VERSION UNKNOWN
#endif

#define _ACK DecId::dec_ack
#define _NAK DecId::dec_nak
#if RAILCOMSPEC_VERSION == 2012
#define _BSY DecId::dec_bsy
#endif
#define _RES DecId::dec_res
#define _INV DecId::dec_inv

const uint8_t decode[UINT8_MAX + 1] = {
    _INV, _INV, _INV, _INV, _INV, _INV, _INV, _INV, // 0x00-0x07
    _INV, _INV, _INV, _INV, _INV, _INV, _INV,       // 0x08-0x0e
#if RAILCOMSPEC_VERSION == 2012
    _NAK, // 0x0f
#elif RAILCOMSPEC_VERSION == 2021
    _ACK, // 0x0f (there are two ACKs)
#endif
    _INV, _INV, _INV, _INV, _INV, _INV, _INV, 0x33, // 0x10-0x17
    _INV, _INV, _INV, 0x34, _INV, 0x35, 0x36, _INV, // 0x18-0x1f
    _INV, _INV, _INV, _INV, _INV, _INV, _INV, 0x3a, // 0x20-0x27
    _INV, _INV, _INV, 0x3b, _INV, 0x3c, 0x37, _INV, // 0x28-0x2f
    _INV, _INV, _INV, 0x3f, _INV, 0x3d, 0x38, _INV, // 0x30-0x37
    _INV, 0x3e, 0x39, _INV,                         // 0x38-0x3b
#if RAILCOMSPEC_VERSION == 2012
    _RES, // 0x3c
#elif RAILCOMSPEC_VERSION == 2021
    _NAK, // 0x3c (optional)
#endif
    _INV, _INV, _INV,                               // 0x3d-0x3f
    _INV, _INV, _INV, _INV, _INV, _INV, _INV, 0x24, // 0x40-0x47
    _INV, _INV, _INV, 0x23, _INV, 0x22, 0x21, _INV, // 0x48-0x4f
    _INV, _INV, _INV, 0x1f, _INV, 0x1e, 0x20, _INV, // 0x50-0x57
    _INV, 0x1d, 0x1c, _INV, 0x1b, _INV, _INV, _INV, // 0x58-0x5f
    _INV, _INV, _INV, 0x19, _INV, 0x18, 0x1a, _INV, // 0x60-0x67
    _INV, 0x17, 0x16, _INV, 0x15, _INV, _INV, _INV, // 0x68-0x6f
    _INV, 0x25, 0x14, _INV, 0x13, _INV, _INV, _INV, // 0x70-0x77
    0x32, _INV, _INV, _INV, _INV, _INV, _INV, _INV, // 0x78-0x7f
    _INV, _INV, _INV, _INV, _INV, _INV, _INV, _RES, // 0x80-0x87
    _INV, _INV, _INV, 0x0e, _INV, 0x0d, 0x0c, _INV, // 0x88-0x8f
    _INV, _INV, _INV, 0x0a, _INV, 0x09, 0x0b, _INV, // 0x90-0x97
    _INV, 0x08, 0x07, _INV, 0x06, _INV, _INV, _INV, // 0x98-0x9f
    _INV, _INV, _INV, 0x04, _INV, 0x03, 0x05, _INV, // 0xa0-0xa7
    _INV, 0x02, 0x01, _INV, 0x00, _INV, _INV, _INV, // 0xa8-0xaf
    _INV, 0x0f, 0x10, _INV, 0x11, _INV, _INV, _INV, // 0xb0-0xb7
    0x12, _INV, _INV, _INV, _INV, _INV, _INV, _INV, // 0xb8-0xbf
    _INV, _INV, _INV, _RES, _INV, 0x2b, 0x30, _INV, // 0xc0-0xc7
    _INV, 0x2a, 0x2f, _INV, 0x31, _INV, _INV, _INV, // 0xc8-0xcf
    _INV, 0x29, 0x2e, _INV, 0x2d, _INV, _INV, _INV, // 0xd0-0xd7
    0x2c, _INV, _INV, _INV, _INV, _INV, _INV, _INV, // 0xd8-0xdf
    _INV,                                           // 0xe0
#if RAILCOMSPEC_VERSION == 2012
    _BSY, // 0xe1
#elif RAILCOMSPEC_VERSION == 2021
    _RES, // 0xe1
#endif
    0x28, _INV, 0x27, _INV, _INV, _INV,             // 0xe2-0xe7
    0x26, _INV, _INV, _INV, _INV, _INV, _INV, _INV, // 0xe8-0xef
    _ACK, _INV, _INV, _INV, _INV, _INV, _INV, _INV, // 0xf0-0xf7
    _INV, _INV, _INV, _INV, _INV, _INV, _INV, _INV, // 0xf8-0xff
};

const char *dyn_name(DynId id)
{
    static constexpr int name_max = 8;
    static const char names[DynId::dyn_max][name_max] = {
        "SPD",   "SPD2",   "ID2",    "ID3",    "ID4",   "ID5",    "INPUT", "STATS", // 0..7
        "CONT1", "CONT2",  "CONT3",  "CONT4",  "CONT5", "CONT6",  "CONT7", "CONT8", // 8..15
        "CONT9", "CONT10", "CONT11", "CONT12", "ADRS",  "STATUS", "ODOM",  "ID23",  // 16..23
        "ID24",  "ID25",   "ID26",   "ID27",   "ID28",  "ID29",   "ID30",  "ID31",  // 24..31
        "ID32",  "ID33",   "ID34",   "ID35",   "ID36",  "ID37",   "ID38",  "ID39",  // 32..39
        "ID40",  "ID41",   "ID42",   "ID43",   "ID44",  "ID45",   "ID46",  "ID47",  // 40..47
        "ID48",  "ID49",   "ID50",   "ID51",   "ID52",  "ID53",   "ID54",  "ID55",  // 48..55
        "ID56",  "ID57",   "ID58",   "ID59",   "ID60",  "ID61",   "ID62",  "ID63",  // 56..63
    };
    assert(id < DynId::dyn_max);
    return names[id];
}

}; // namespace RailComSpec
