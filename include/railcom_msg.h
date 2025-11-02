#pragma once

#include <cstdint>
#include "railcom_spec.h"

struct RailComMsg
{

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

    bool operator==(const RailComMsg& rhs) const
    {
        if (id != rhs.id)
            return false;

        switch (id) {

            case MsgId::ack:
            case MsgId::nak:
            case MsgId::bsy:
                return true;

            case MsgId::pom:
                return pom.val == rhs.pom.val;

            case MsgId::ahi:
                return ahi.ahi == rhs.ahi.ahi;

            case MsgId::alo:
                return alo.alo == rhs.alo.alo;

            case MsgId::ext:
                return ext.typ == rhs.ext.typ && ext.pos == rhs.ext.pos;

            case MsgId::dyn:
                return dyn.id == rhs.dyn.id && dyn.val == rhs.dyn.val;

            case MsgId::xpom:
                return xpom.ss == rhs.xpom.ss &&
                       xpom.val[0] == rhs.xpom.val[0] &&
                       xpom.val[1] == rhs.xpom.val[1] &&
                       xpom.val[2] == rhs.xpom.val[2] &&
                       xpom.val[3] == rhs.xpom.val[3];

            default:
                return false;

        } // switch (id)
    }

}; // struct RailComMsg