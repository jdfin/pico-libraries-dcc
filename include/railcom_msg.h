#pragma once

#include <cstdint>
#include "railcom_spec.h"

struct RailComMsg {

    // RailCom message IDs.
    // These are not from the RailCom spec, used here as sort of a union
    // between RailComSpec::PktId and the specially-encoded bytes in DecId.
    enum class MsgId : uint8_t {
        ack, nak, bsy, // special encoding
        pom, ahi, alo, ext, dyn, xpom, // real packets
        inv, // unset
    };

    const char* id_name() const;

    MsgId id;
    union {
        struct {
            uint8_t val;
        } pom;
        struct {
            uint8_t ahi;
        } ahi;
        struct {
            uint8_t alo;
        } alo;
        struct {
            uint8_t typ; // 6 bits
            uint8_t pos; // 8 bits
        } ext;
        struct {
            RailComSpec::DynId id;
            uint8_t val; // 8 bits
        } dyn;
        struct {
            uint8_t ss; // 2 bits
            uint8_t val[4]; // 8 bits each
        } xpom;
    };

    // extract message from 6-bit data buffer
    bool parse1(const uint8_t*& d, const uint8_t* d_end);
    bool parse2(const uint8_t*& d, const uint8_t* d_end);

    // pretty-print to buf
    int show(char* buf, int buf_len) const;

}; // class RailComMsg