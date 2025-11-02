#pragma once

#include <cstdint>
#include <cstring>
#include "railcom_spec.h"
#include "hardware/uart.h"


class RailCom
{
public:
    RailCom(uart_inst_t* uart, int rx_gpio);

    void reset()
    {
        if (_uart == nullptr || _rx_gpio < 0)
            return;
        uart_deinit(_uart);
        uart_init(_uart, RailComSpec::baud);
    }

    void read();

    void parse();

    char *dump(char *buf, int buf_len) const; // raw

    char *show(char *buf, int buf_len) const; // pretty

private:

    uart_inst_t* _uart;

    int _rx_gpio;

    ///// Raw RailCom Data (4/8 encoded, and decoded bytes)

    static constexpr int pkt_max = RailComSpec::ch1_bytes + RailComSpec::ch2_bytes; // for both enc and dec
    uint8_t _enc[pkt_max]; // encoded (4/8 code)
    uint8_t _dec[pkt_max]; // decoded (6 bits per byte) from decode[]
    int _pkt_len; // _enc[] and _dec[] are the same length

    ///// Parsed RailCom Packets

    struct RcMsg {

        // RailCom message IDs.
        // These are not from the RailCom spec, used here as sort of a union
        // between RailComSpec::PktId and the specially-encoded bytes in DecId.
        enum class MsgId : uint8_t {
            ack, nak, bsy, // special encoding
            pom, ahi, alo, ext, dyn, xpom, // real packets
            inv, // unset
        };

        const char *id_name() const;

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
        bool parse1(const uint8_t *&d, const uint8_t *d_end);
        bool parse2(const uint8_t *&d, const uint8_t *d_end);
        // pretty-print to buf
        int show(char *buf, int buf_len) const;
    };

    // channel 1 messages
    RcMsg _ch1_msg;
    int _ch1_msg_cnt; // 0 or 1

    // channel 2 messages
    // at most one message per byte (e.g. all ACK)
    static constexpr int ch2_msg_max = RailComSpec::ch2_bytes;
    RcMsg _ch2_msg[ch2_msg_max];
    int _ch2_msg_cnt;

}; // class RailCom