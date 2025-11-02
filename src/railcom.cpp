
#include "railcom.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "xassert.h"

#include "dbg_gpio.h"

static const int dbg_gpio = 21;

RailCom::RailCom(uart_inst_t* uart, int rx_gpio) :
    _uart(uart),
    _rx_gpio(rx_gpio)
{
    if (_uart == nullptr || _rx_gpio < 0)
        return;

    if (dbg_gpio >= 0)
        DbgGpio::init(dbg_gpio);

    gpio_set_function(_rx_gpio, UART_FUNCSEL_NUM(_uart, _rx_gpio));
    uart_init(_uart, RailComSpec::baud);

    _pkt_len = 0;
    _ch1_msg_cnt = 0;
    _ch2_msg_cnt = 0;
}

void RailCom::read()
{
    _pkt_len = 0;
    _ch1_msg_cnt = 0;
    _ch2_msg_cnt = 0;

    for (_pkt_len = 0; _pkt_len < pkt_max && uart_is_readable(_uart); _pkt_len++) {
        _enc[_pkt_len] = uart_getc(_uart);
        _dec[_pkt_len] = RailComSpec::decode[_enc[_pkt_len]];
        if (_dec[_pkt_len] == RailComSpec::DecId::dec_inv) {
#if 0
            if (dbg_gpio >= 0) {
                DbgGpio d(dbg_gpio); // scope trigger
                // this seems to be needed to force construction of DbgGpio
                [[maybe_unused]] volatile int i = 0;
            }
#endif
        }
    }
#if 0
    if (_pkt_len != pkt_max) {
        DbgGpio d(dbg_gpio);
        [[maybe_unused]] volatile int i = 0;
    }
#endif
}

// Split received packet into channel 1 and channel 2
//
// Channel 1 is by default always sent by all decoders that support RailCom,
// but that can be disabled in the decoder. If there is more than one loco on
// the same track, they will both send channel 1 and it will likely be junk.
// We don't use it, but decoding it helps figure out where channel 2 starts.
//
// Channel 2 is only sent by the DCC-addressed decoder. If there is no decoder
// at the DCC address of DCC packet, there will be no channel 2 data. If there
// is an addressed decoder, it will send channel 2 data, but it is often
// corrupted, presumably by dirty track and such. Observed corruption seems to
// extra ones in the 4/8 encoding, implying the decoder was trying to send a
// zero (>10 mA), but it did not get through (e.g. because of dirty track).
// Multiple decoders at the same DCC address would also cause corruption, but
// with excess zeros instead of excess ones. Channel 2 often does not need
// the full 6 bytes. It would be possible to use information from channel 2
// with only one byte, e.g. an ack, but a choice here is to require 6 valid
// bytes to consider anything in channel 2 valid.
//
// If we got at least 2 bytes, see if the first two are valid channel 1 data.
//
// If we got a valid channel 1,
//   try extracting 6 bytes of channel 2 data starting at byte 2,
// else (channel 1 invalid),
//   try extracting 6 bytes of channel 2 data starting at byte 0.
//
// If channel 1 failed due to corruption, channel 2 will also fail due to
// corruption. If channel 1 failed because no decoder sent it (i.e. it is
// disabled), we probably got 6 bytes and it's all channel 2.
//
// XXX Can we get less than 6 bytes of channel 2 data? ESU LokSound 5 fills
//     out channel 2 to 6 bytes, but I don't think the spec requires that.
//
void RailCom::parse()
{
    const uint8_t *d = _dec;
    const uint8_t *d_end = d + _pkt_len;

    // Attempt to extract channel 1.
    //
    // It must be the first two bytes, and it must contain either an alo or
    // ahi message. Anything else, and we try to parse as channel 2 below.
    // It is possible no decoder sent any channel 1 data and we only have 6
    // bytes of channel 2. It is also possible we have good channel 1 data,
    // but channel 2 not there or is corrupted; we still use channel 1.

    if (_ch1_msg.parse1(d, d_end))
        _ch1_msg_cnt = 1;
    else
        _ch1_msg_cnt = 0;

    // Attempt to extract channel 2.
    //
    // We must have exactly 6 bytes available to look at - either channel 1
    // was parsed above and we have 6 left, or we have 6 total because no
    // decoder sent channel 1. All 6 bytes must decode correctly. If channel 1
    // was corrupted, we won't get channel 2 because we'll restart here with
    // the corrupt data. If there's anything in channel 2 we don't understand,
    // we don't use any of it. (An alternative would be to use what we can
    // understand.)


    _ch2_msg_cnt = 0;
    if ((d_end - d) == RailComSpec::ch2_bytes) {
        while (d < d_end) {
            xassert(_ch2_msg_cnt < ch2_msg_max);
            if (_ch2_msg[_ch2_msg_cnt].parse2(d, d_end)) {
                _ch2_msg_cnt++;
            } else {
                _ch2_msg_cnt = 0;
                break;
            }
        }
    }
}

// Extract one message from decoded 6-bit data
// Return true if message extracted, false on error
// Update d to point to next unused data

// channel 1 messages
bool RailCom::RcMsg::parse1(const uint8_t *&d, const uint8_t *d_end)
{
    int len = d_end - d; // 6-bit datum available
    if (len < 1)
        return false;

    uint8_t b0 = d[0];
    if (b0 < RailComSpec::DecId::dec_max) {
        // b0 was successfully decoded to 6 bits of data
        RailComSpec::PktId pkt_id = RailComSpec::PktId((b0 >> 2) & 0x0f);
        if (pkt_id == RailComSpec::PktId::pkt_ahi) {
            // 12 bit (2 byte) message
            if (len >= 2) {
                id = RcMsg::MsgId::ahi;
                ahi.ahi = ((b0 << 6) | d[1]) & 0xff;
                d += 2;
                return true;
            }
        } else if (pkt_id == RailComSpec::PktId::pkt_alo) {
            // 12 bit (2 byte) message
            if (len >= 2) {
                id = RcMsg::MsgId::alo;
                alo.alo = ((b0 << 6) | d[1]) & 0xff;
                d += 2;
                return true;
            }
        }
    }
    return false;
}

// channel 2 messages
bool RailCom::RcMsg::parse2(const uint8_t *&d, const uint8_t *d_end)
{
    int len = d_end - d; // 6-bit datum available
    if (len < 1)
        return false;

    uint8_t b0 = d[0];
    if (b0 == RailComSpec::DecId::dec_ack) {
        id = RcMsg::MsgId::ack;
        d += 1;
        return true;
    } else if (b0 == RailComSpec::DecId::dec_nak) {
        id = RcMsg::MsgId::nak;
        d += 1;
        return true;
#if RAILCOMSPEC_VERSION == 2012
    } else if (b0 == RailComSpec::DecId::dec_bsy) {
        id = RcMsg::MsgId::bsy;
        d += 1;
        return true;
#endif
    } else if (b0 < RailComSpec::DecId::dec_max) {
        // b0 was successfully decoded to 6 bits of data
        RailComSpec::PktId pkt_id = RailComSpec::PktId((b0 >> 2) & 0x0f);
        if (pkt_id == RailComSpec::PktId::pkt_pom) {
            // 12 bit (2 byte) message
            if (len >= 2) {
                id = RcMsg::MsgId::pom;
                pom.val = ((b0 << 6) | d[1]) & 0xff;
                d += 2;
                return true;
            }
        // it looks like ahi and alo are allowed in either channel
        } else if (pkt_id == RailComSpec::PktId::pkt_ahi) {
            // 12 bit (2 byte) message
            if (len >= 2) {
                id = RcMsg::MsgId::ahi;
                ahi.ahi = ((b0 << 6) | d[1]) & 0xff;
                d += 2;
                return true;
            }
        } else if (pkt_id == RailComSpec::PktId::pkt_alo) {
            // 12 bit (2 byte) message
            if (len >= 2) {
                id = RcMsg::MsgId::alo;
                alo.alo = ((b0 << 6) | d[1]) & 0xff;
                d += 2;
                return true;
            }
        } else if (pkt_id == RailComSpec::PktId::pkt_ext) {
            // 18 bit (3 byte) message
            if (len >= 3) {
                id = RcMsg::MsgId::ext;
                ext.typ = ((b0 << 4) & 0x30) | ((d[1] >> 2) & 0x0f);
                ext.pos = ((d[1] << 6) & 0xc0) | d[2];
                d += 3;
                return true;
            }
        } else if (pkt_id == RailComSpec::PktId::pkt_dyn) {
            // 18 bit (3 byte) message
            if (len >= 3) {
                id = RcMsg::MsgId::dyn;
                dyn.val = ((b0 << 6) | d[1]) & 0xff;
                dyn.id = RailComSpec::DynId(d[2]);
                d += 3;
                return true;
            }
        } else if ((pkt_id & 0x0c) == RailComSpec::PktId::pkt_xpom) {
            // xpom 8, 9, 10, 11 (0x08, 0x09, 0x0a, 0x0b)
            // 36 bit (6 byte) message
            if (len >= 6) {
                // [ d0 ] [ d1 ] [ d2 ] [ d3 ] [ d4 ] [ d5 ]
                // IIII00 000000 111111 112222 222233 333333
                //     [ val0  ] [ val1  ][ val2  ][ val3  ]
                id = RcMsg::MsgId::xpom;
                xpom.ss = pkt_id & 0x03;
                xpom.val[0] = (b0 << 6) | d[1];
                xpom.val[1] = (d[2] << 2) | (d[3] >> 4);
                xpom.val[2] = (d[3] << 4) | (d[4] >> 2);
                xpom.val[3] = (d[4] << 6) | d[5];
                d += 6;
                return true;
            }
        }
    }
    return false;
}

// for each byte:
//   if a byte is not 4/8 valid
//     show raw !hh!
//   else
//     decode to 6 bits
//     if >= 0x40
//       show raw [hh]
//     else
//       show decoded bbbbbb
char* RailCom::dump(char* buf, int buf_len) const
{
    memset(buf, '\0', buf_len);

    char* b = buf;
    char* e = buf + buf_len;

    b += snprintf(b, e - b, "R ");

    for (int i = 0; i < _pkt_len; i++) {
        if (_dec[i] < RailComSpec::DecId::dec_max) {
            // encoded value represents valid data - print decoded value in binary
            for (uint8_t m = 0x20; m != 0; m >>= 1)
                b += snprintf(b, e - b, "%c", (_dec[i] & m) != 0 ? '1' : '0');
        } else if (_dec[i] == RailComSpec::DecId::dec_ack) {
            b += snprintf(b, e - b, "AK");
        } else if (_dec[i] == RailComSpec::DecId::dec_nak) {
            b += snprintf(b, e - b, "NK");
#if RAILCOMSPEC_VERSION == 2012
        } else if (_dec[i] == RailComSpec::DecId::dec_bsy) {
            b += snprintf(b, e - b, "BZ");
#endif
        } else {
            // print encoded value in hex
            b += snprintf(b, e - b, "%02x", _enc[i]);
        }
        if (i < (_pkt_len - 1))
            b += snprintf(b, e - b, " ");
    }

    return buf;
}

// Pretty-print to buf
// Return number of characters written to buf
// Example output:
//   "[ACK]"
//   "[POM 1e]"
//   "[ALO 03]"
//   "[DYN SPD=0]"
int RailCom::RcMsg::show(char* buf, int buf_len) const
{
    memset(buf, '\0', buf_len);

    char* b = buf;
    char* e = buf + buf_len;

    b += snprintf(b, e - b, "[");

    b += snprintf(b, e - b, id_name());

    if (id == MsgId::ack || id == MsgId::nak || id == MsgId::bsy) {
        // nothing more to print
    } else if (id == MsgId::pom) {
        b += snprintf(b, e - b, " %02x", pom.val);
    } else if (id == MsgId::ahi) {
        b += snprintf(b, e - b, " %02x", ahi.ahi);
    } else if (id == MsgId::alo) {
        b += snprintf(b, e - b, " %02x", alo.alo);
    } else if (id == MsgId::ext) {
        b += snprintf(b, e - b, " %02x %02x", ext.typ, ext.pos);
    } else if (id == MsgId::dyn) {
        b += snprintf(b, e - b, " %s=%d", RailComSpec::dyn_name(dyn.id), dyn.val);
    } else if (id == MsgId::xpom) {
        b += snprintf(b, e - b, " %d %02x %02x %02x %02x", xpom.ss, xpom.val[0], xpom.val[1], xpom.val[2], xpom.val[3]);
    } else {
        b += snprintf(b, e - b, " ?");
    }

    b += snprintf(b, e - b, "]");

    return b - buf;
}

// return argument buf so it can e.g. be a printf argument
char* RailCom::show(char* buf, int buf_len) const
{
    memset(buf, '\0', buf_len);

    char* b = buf;
    char* e = buf + buf_len;

    b += snprintf(b, e - b, "R ");

    // it is expected that parse() has been called before this
    // XXX Enforce!

    // show channel 1
    if (_ch1_msg_cnt > 0) {
        b += _ch1_msg.show(b, e - b);
        b += snprintf(b, e - b, " ");
    }

    // show channel 2
    for (int i = 0; i < _ch2_msg_cnt; i++) {
        b += _ch2_msg[i].show(b, e - b);
        b += snprintf(b, e - b, " ");
    }

    return buf;

} // RailCom::show()

const char *RailCom::RcMsg::id_name() const
{
    static constexpr uint id_max = uint(MsgId::inv) + 1;
    static constexpr uint name_max = 8;
    static char names[id_max][name_max] = {
        "ACK", "NAK", "BSY", "POM", "AHI", "ALO", "EXT", "DYN", "XPOM", "INV"
    };
    xassert(uint(id) < id_max);
    return names[uint(id)];
}