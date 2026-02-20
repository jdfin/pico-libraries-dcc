#include <cstdio>
#include <cstdint>
#include <cstring>

#include "dcc_bit.h"
#include "dcc_pkt.h"
#include "dcc_spec.h"
#include "test.h"

// --- to_half classification ---

static bool test_half_bit_one()
{
    // One-bit: 52-64us
    if (DccBit::to_half(52) != 1) return false;
    if (DccBit::to_half(58) != 1) return false;
    if (DccBit::to_half(64) != 1) return false;

    // Just outside
    if (DccBit::to_half(51) == 1) return false;
    if (DccBit::to_half(65) == 1) return false;

    return true;
}

static bool test_half_bit_zero()
{
    // Zero-bit: 90-10000us
    if (DccBit::to_half(90) != 0) return false;
    if (DccBit::to_half(100) != 0) return false;
    if (DccBit::to_half(10000) != 0) return false;

    // Just outside
    if (DccBit::to_half(89) == 0) return false;
    if (DccBit::to_half(10001) == 0) return false;

    return true;
}

static bool test_half_bit_invalid()
{
    // In the gap between one and zero
    if (DccBit::to_half(70) != 2) return false;
    if (DccBit::to_half(80) != 2) return false;

    // Way too short
    if (DccBit::to_half(10) != 2) return false;

    // Way too long
    if (DccBit::to_half(20000) != 2) return false;

    return true;
}

// --- Helper: generate edge timestamps for a DCC packet ---

// Generate a sequence of edges for a given packet byte array.
// Writes edge timestamps into `edges` and returns the number of edges.
// The edges simulate: preamble (one-bits), start-bit, bytes with
// start/stop bits, all using nominal timings.
// Generate edges for a DCC packet.
// The decoder measures intervals between consecutive edges.
// A one-half is ~58us; a zero-half is ~100us.
// Each complete bit is two matching half-bits.
static int make_edges(const uint8_t *msg, int msg_len, int preamble_bits,
                      uint64_t *edges, int edges_max)
{
    int e = 0;
    uint64_t t = 1000; // start time

    auto add = [&](int us) {
        t += us;
        if (e < edges_max)
            edges[e++] = t;
    };

    // First edge to initialize the decoder (no interval yet)
    if (e < edges_max)
        edges[e++] = t;

    int t1 = DccSpec::t1_nom_us;
    int t0 = DccSpec::t0_nom_us;

    // Preamble: each one-bit is two half-one intervals
    for (int i = 0; i < preamble_bits; i++) {
        add(t1);
        add(t1);
    }

    // For each byte: start bit (0), 8 data bits
    for (int b = 0; b < msg_len; b++) {
        // Start bit (zero)
        add(t0);
        add(t0);

        // 8 data bits, MSB first
        for (int bit = 7; bit >= 0; bit--) {
            int h = ((msg[b] >> bit) & 1) ? t1 : t0;
            add(h);
            add(h);
        }
    }

    // Stop bit (one-bit) after last byte
    add(t1);
    add(t1);

    return e;
}

// --- Preamble detection ---

// Captured packet storage for callback
static uint8_t captured_pkt[16];
static int captured_pkt_len;
static int captured_preamble_len;
static int captured_count;

static void pkt_callback(const uint8_t *pkt, int pkt_len, int preamble_len,
                          uint64_t /*start_us*/, int /*bad_cnt*/)
{
    captured_pkt_len = pkt_len < (int)sizeof(captured_pkt) ? pkt_len : (int)sizeof(captured_pkt);
    memcpy(captured_pkt, pkt, captured_pkt_len);
    captured_preamble_len = preamble_len;
    captured_count++;
}

static bool test_preamble_and_packet()
{
    // Build an idle packet: ff 00 ff
    DccPktIdle idle;
    uint8_t msg[3] = {idle.data(0), idle.data(1), idle.data(2)};

    uint64_t edges[512];
    int n = make_edges(msg, 3, 14, edges, 512);

    DccBit decoder(0);
    captured_count = 0;
    captured_pkt_len = 0;
    decoder.on_pkt_recv(pkt_callback);
    decoder.init();

    for (int i = 0; i < n; i++) {
        decoder.edge(edges[i]);
    }

    if (captured_count != 1) return false;
    if (captured_pkt_len != 3) return false;
    if (captured_pkt[0] != 0xff) return false;
    if (captured_pkt[1] != 0x00) return false;
    if (captured_pkt[2] != 0xff) return false;
    if (captured_preamble_len < 14) return false;

    return true;
}

static bool test_full_packet_decode()
{
    // Speed128 packet for address 3, speed 50
    DccPktSpeed128 spd(3, 50);
    uint8_t msg[4];
    for (int i = 0; i < 4; i++) msg[i] = spd.data(i);

    uint64_t edges[512];
    int n = make_edges(msg, 4, 14, edges, 512);

    DccBit decoder(0);
    captured_count = 0;
    captured_pkt_len = 0;
    decoder.on_pkt_recv(pkt_callback);
    decoder.init();

    for (int i = 0; i < n; i++) {
        decoder.edge(edges[i]);
    }

    if (captured_count != 1) return false;
    if (captured_pkt_len != 4) return false;

    // verify bytes match
    for (int i = 0; i < 4; i++) {
        if (captured_pkt[i] != msg[i]) return false;
    }

    // verify it's a valid DCC packet
    if (!DccPkt::check_xor(captured_pkt, captured_pkt_len)) return false;
    if (DccPkt::decode_type(captured_pkt, captured_pkt_len) != DccPkt::Speed128)
        return false;

    return true;
}

static bool test_short_preamble_rejection()
{
    // Use only 8 one-bits (need at least 10)
    DccPktIdle idle;
    uint8_t msg[3] = {idle.data(0), idle.data(1), idle.data(2)};

    uint64_t edges[512];
    int n = make_edges(msg, 3, 8, edges, 512);

    DccBit decoder(0);
    captured_count = 0;
    decoder.on_pkt_recv(pkt_callback);
    decoder.init();

    for (int i = 0; i < n; i++) {
        decoder.edge(edges[i]);
    }

    // Should NOT have decoded a packet (preamble too short)
    if (captured_count != 0) return false;

    return true;
}

static bool test_invalid_timing_rejection()
{
    DccBit decoder(0);
    captured_count = 0;
    decoder.on_pkt_recv(pkt_callback);
    decoder.init();

    // Feed edges with invalid timing (in the gap 65-89us)
    uint64_t t = 1000;
    for (int i = 0; i < 50; i++) {
        decoder.edge(t);
        t += 75; // invalid timing
    }

    if (captured_count != 0) return false;

    return true;
}

extern const Test tests_dcc_bit[] = {
    {"half_bit_one", test_half_bit_one},
    {"half_bit_zero", test_half_bit_zero},
    {"half_bit_invalid", test_half_bit_invalid},
    {"preamble_and_packet", test_preamble_and_packet},
    {"full_packet_decode", test_full_packet_decode},
    {"short_preamble_rejection", test_short_preamble_rejection},
    {"invalid_timing_rejection", test_invalid_timing_rejection},
};

extern const int tests_dcc_bit_cnt = sizeof(tests_dcc_bit) / sizeof(tests_dcc_bit[0]);
