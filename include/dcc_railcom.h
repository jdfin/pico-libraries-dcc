#pragma once

#include <cstdint>
#include "hardware/uart.h"


class DccRailcom
{
public:
    DccRailcom(uart_inst_t* uart, int rx_gpio);

    void reset()
    {
        if (_uart == nullptr || _rx_gpio < 0)
            return;
        uart_deinit(_uart);
        uart_init(_uart, baud);
    }

    void read();

    inline bool got_pkt() const
    {
        return _pkt_len == pkt_max;
    }

    char *dump(char *buf, int buf_len) const; // raw

    char *show(char *buf, int buf_len) const; // cooked

private:
    uart_inst_t* _uart;
    int _rx_gpio;

    static const uint baud = 250'000;
    static const int pkt_max = 8;
    uint8_t _pkt[pkt_max];
    int _pkt_len;

    // Lookup table: maps 4/8 code (as index) to decoded value
    // Usage: uint8_t value = decode[uint8_t four_eight_code];
    // invalid codes return 0xff.
    static const uint8_t nack = 0x40;
    static const uint8_t ack = 0x41;
    static const uint8_t busy = 0x42;
    static const uint8_t resv = 0x43;  // reserved
    static const uint8_t inv = 0xff;
    static const uint8_t decode[UINT8_MAX + 1];
};