
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
        if (i > 0 && _ch2_msg[i] == _ch2_msg[i-1])
            b += snprintf(b, e - b, "#"); // same as previous message
        else
            b += _ch2_msg[i].show(b, e - b); // different from previous message
        if (i < (_ch2_msg_cnt - 1))
            b += snprintf(b, e - b, " ");
    }

    return buf;

} // RailCom::show()