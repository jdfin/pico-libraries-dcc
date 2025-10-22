
#include "dcc_railcom.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

//#define RAILCOM_VERSION 2012
#define RAILCOM_VERSION 2021

#if (RAILCOM_VERSION != 2012) && (RAILCOM_VERSION != 2021)
#error RAILCOM_VERSION UNKNOWN
#endif

const uint8_t DccRailcom::decode[UINT8_MAX + 1] = {
    inv, inv, inv, inv, inv, inv, inv, inv, // 0x00-0x07
    inv, inv, inv, inv, inv, inv, inv, // 0x08-0x0e
#if RAILCOM_VERSION == 2012
    nack, // 0x0f
#elif RAILCOM_VERSION == 2021
    ack, // 0x0f (there are two ACKs)
#endif
    inv, inv, inv, inv, inv, inv, inv, 0x33, // 0x10-0x17
    inv, inv, inv, 0x34, inv, 0x35, 0x36, inv, // 0x18-0x1f
    inv, inv, inv, inv, inv, inv, inv, 0x3a, // 0x20-0x27
    inv, inv, inv, 0x3b, inv, 0x3c, 0x37, inv, // 0x28-0x2f
    inv, inv, inv, 0x3f, inv, 0x3d, 0x38, inv, // 0x30-0x37
    inv, 0x3e, 0x39, inv, // 0x38-0x3b
#if RAILCOM_VERSION == 2012
    resv, // 0x3c
#elif RAILCOM_VERSION == 2021
    nack, // 0x3c (optional)
#endif
    inv, inv, inv,   // 0x3d-0x3f
    inv, inv, inv, inv, inv, inv, inv, 0x24,  // 0x40-0x47
    inv, inv, inv, 0x23, inv, 0x22, 0x21, inv,   // 0x48-0x4f
    inv, inv, inv, 0x1f, inv, 0x1e, 0x20, inv,   // 0x50-0x57
    inv, 0x1d, 0x1c, inv, 0x1b, inv, inv, inv,   // 0x58-0x5f
    inv, inv, inv, 0x19, inv, 0x18, 0x1a, inv,   // 0x60-0x67
    inv, 0x17, 0x16, inv, 0x15, inv, inv, inv,   // 0x68-0x6f
    inv, 0x25, 0x14, inv, 0x13, inv, inv, inv,   // 0x70-0x77
    0x32, inv, inv, inv, inv, inv, inv, inv,   // 0x78-0x7f
    inv, inv, inv, inv, inv, inv, inv, resv, // 0x80-0x87
    inv, inv, inv, 0x0e, inv, 0x0d, 0x0c, inv,   // 0x88-0x8f
    inv, inv, inv, 0x0a, inv, 0x09, 0x0b, inv,   // 0x90-0x97
    inv, 0x08, 0x07, inv, 0x06, inv, inv, inv,   // 0x98-0x9f
    inv, inv, inv, 0x04, inv, 0x03, 0x05, inv,   // 0xa0-0xa7
    inv, 0x02, 0x01, inv, 0x00, inv, inv, inv,   // 0xa8-0xaf
    inv, 0x0f, 0x10, inv, 0x11, inv, inv, inv,   // 0xb0-0xb7
    0x12, inv, inv, inv, inv, inv, inv, inv,   // 0xb8-0xbf
    inv, inv, inv, resv, inv, 0x2b, 0x30, inv,   // 0xc0-0xc7
    inv, 0x2a, 0x2f, inv, 0x31, inv, inv, inv,   // 0xc8-0xcf
    inv, 0x29, 0x2e, inv, 0x2d, inv, inv, inv,   // 0xd0-0xd7
    0x2c, inv, inv, inv, inv, inv, inv, inv,   // 0xd8-0xdf
    inv, // 0xe0
#if RAILCOM_VERSION == 2012
    busy, // 0xe1
#elif RAILCOM_VERSION == 2021
    resv, // 0xe1
#endif
    0x28, inv, 0x27, inv, inv, inv,   // 0xe2-0xe7
    0x26, inv, inv, inv, inv, inv, inv, inv,   // 0xe8-0xef
    ack, inv, inv, inv, inv, inv, inv, inv,   // 0xf0-0xf7
    inv, inv, inv, inv, inv, inv, inv, inv,   // 0xf8-0xff
};


DccRailcom::DccRailcom(uart_inst_t* uart, int rx_gpio) :
    _uart(uart),
    _rx_gpio(rx_gpio)
{
    if (_uart == nullptr || _rx_gpio < 0)
        return;

    gpio_set_function(_rx_gpio, UART_FUNCSEL_NUM(_uart, _rx_gpio));
    uart_init(_uart, baud);
}

void DccRailcom::read()
{
    for (_pkt_len = 0; _pkt_len < pkt_max && uart_is_readable(_uart); _pkt_len++)
        _pkt[_pkt_len] = uart_getc(_uart);
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
char* DccRailcom::dump(char* buf, int buf_len) const
{
    memset(buf, '\0', buf_len);

    char* b = buf;
    char* e = buf + buf_len;

    for (int i = 0; i < _pkt_len; i++) {
        if (i > 0)
            b += snprintf(b, e - b, " ");
        uint8_t d = decode[_pkt[i]];
        if (d == inv) {
            b += snprintf(b, e - b, "%02x", _pkt[i]);
        } else if (d >= 0x40) {
            b += snprintf(b, e - b, "%02x", _pkt[i]);
        } else {
            for (uint8_t m = 0x20; m != 0; m >>= 1) {
                b += snprintf(b, e - b, "%c", (d & m) != 0 ? '1' : '0');
            }
        }
    }

    return buf;
}

char* DccRailcom::show(char* buf, int buf_len) const
{
    memset(buf, '\0', buf_len);

    if (!got_pkt())
        return buf;

    char* b = buf;
    char* e = buf + buf_len;

#if 0
    // raw 4/8 encoded data
    for (int i = 0; i < pkt_max; i++)
        b += snprintf(b, e - b, "%02x ", uint(_pkt[i]));
    b += snprintf(b, e - b, "| ");
#endif

    // decode (to 6 bits per byte)
    uint8_t data[pkt_max];
    for (int i = 0; i < pkt_max; i++)
        data[i] = decode[_pkt[i]];

#if 0
    // decoded data (6 bits per byte)
    for (int i = 0; i < pkt_max; i++)
        b += snprintf(b, e - b, "%02x ", uint(data[i]));
    b += snprintf(b, e - b, "| ");
#endif

    // channel 1
    if (data[0] < 0x40 && data[1] < 0x40) {
        uint id = (data[0] >> 2) & 0x0f;
        uint d0 = ((data[0] << 6) | data[1]) & 0xff;
        b += snprintf(b, e - b, "%u: %02x ", id, d0);
    } else {
        b += snprintf(b, e - b, "corrupt ");
    }
    b += snprintf(b, e - b, "| ");

    // channel 2
    if (data[2] >= 0x40) {
        if (data[2] == ack) {
            b += snprintf(b, e - b, "ack ");
        } else if (data[2] == nack) {
            b += snprintf(b, e - b, "nack ");
        } else if (data[2] == busy) {
            b += snprintf(b, e - b, "busy ");
        } else {
            b += snprintf(b, e - b, "%02x ", uint(data[2]));
        }
    } else {
        uint id = (data[2] >> 2) & 0x0f;
        uint d0 = ((data[2] << 6) | data[3]) & 0xff;
        if (id == 0) {
            b += snprintf(b, e - b, "%u: %02x [%02x %02x %02x %02x]", id, d0, data[4], data[5], data[6], data[7]);
        } else {
            uint d1 = ((data[4] << 2) | (data[5] >> 4)) & 0xff;
            uint d2 = ((data[5] << 4) | (data[6] >> 2)) & 0xff;
            uint d3 = ((data[6] << 6) | data[7]) & 0xff;
            b += snprintf(b, e - b, "%u: %02x %02x %02x %02x ", id, d0, d1, d2, d3);
        }
    }

    return buf;

} // DccRailcom::show()