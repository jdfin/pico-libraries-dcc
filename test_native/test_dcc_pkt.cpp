#include <cstdio>
#include <cstring>

#include "dcc_pkt.h"
#include "test.h"

// --- Idle / Reset ---

static bool test_idle_packet()
{
    DccPktIdle pkt;
    if (pkt.msg_len() != 3) return false;
    if (pkt.data(0) != 0xff) return false;
    if (pkt.data(1) != 0x00) return false;
    if (!pkt.check_xor()) return false;
    if (pkt.get_type() != DccPkt::Idle) return false;
    return true;
}

static bool test_reset_packet()
{
    DccPktReset pkt;
    if (pkt.msg_len() != 3) return false;
    if (pkt.data(0) != 0x00) return false;
    if (pkt.data(1) != 0x00) return false;
    if (pkt.data(2) != 0x00) return false;
    if (!pkt.check_xor()) return false;
    if (pkt.get_type() != DccPkt::Reset) return false;
    return true;
}

// --- Short address ---

static bool test_short_address()
{
    DccPktSpeed128 pkt(3, 0);
    if (pkt.get_address() != 3) return false;
    if (pkt.msg_len() != 4) return false; // 1 addr + inst + speed + xor

    DccPktSpeed128 pkt2(127, 0);
    if (pkt2.get_address() != 127) return false;
    if (pkt2.msg_len() != 4) return false;

    // address 1
    DccPktSpeed128 pkt3(1, 0);
    if (pkt3.get_address() != 1) return false;

    return true;
}

// --- Long address ---

static bool test_long_address()
{
    DccPktSpeed128 pkt(128, 0);
    if (pkt.get_address() != 128) return false;
    if (pkt.msg_len() != 5) return false; // 2 addr + inst + speed + xor

    DccPktSpeed128 pkt2(10239, 0);
    if (pkt2.get_address() != 10239) return false;
    if (pkt2.msg_len() != 5) return false;

    DccPktSpeed128 pkt3(256, 0);
    if (pkt3.get_address() != 256) return false;

    return true;
}

// --- Speed128 encode/decode ---

static bool test_speed128_forward()
{
    DccPktSpeed128 pkt(3, 50);
    if (pkt.get_speed() != 50) return false;
    if (!pkt.check_xor()) return false;

    // int_to_dcc / dcc_to_int round-trip for forward
    for (int s = 0; s <= 127; s++) {
        uint8_t dcc = DccPktSpeed128::int_to_dcc(s);
        int back = DccPktSpeed128::dcc_to_int(dcc);
        if (back != s) return false;
    }
    return true;
}

static bool test_speed128_reverse()
{
    DccPktSpeed128 pkt(3, -50);
    if (pkt.get_speed() != -50) return false;
    if (!pkt.check_xor()) return false;

    // int_to_dcc / dcc_to_int round-trip for reverse
    for (int s = -127; s <= -1; s++) {
        uint8_t dcc = DccPktSpeed128::int_to_dcc(s);
        int back = DccPktSpeed128::dcc_to_int(dcc);
        if (back != s) return false;
    }
    return true;
}

static bool test_speed128_set_get()
{
    DccPktSpeed128 pkt(3, 0);
    if (pkt.get_speed() != 0) return false;

    pkt.set_speed(100);
    if (pkt.get_speed() != 100) return false;
    if (!pkt.check_xor()) return false;

    pkt.set_speed(-80);
    if (pkt.get_speed() != -80) return false;
    if (!pkt.check_xor()) return false;

    return true;
}

// --- Function packets ---

static bool test_func0()
{
    DccPktFunc0 pkt(3);

    // all off initially
    for (int f = 0; f <= 4; f++) {
        if (pkt.get_f(f)) return false;
    }

    // set each on individually
    for (int f = 0; f <= 4; f++) {
        pkt.set_f(f, true);
        if (!pkt.get_f(f)) return false;
        if (!pkt.check_xor()) return false;
        pkt.set_f(f, false);
        if (pkt.get_f(f)) return false;
    }

    // set all on
    for (int f = 0; f <= 4; f++)
        pkt.set_f(f, true);
    for (int f = 0; f <= 4; f++) {
        if (!pkt.get_f(f)) return false;
    }
    if (!pkt.check_xor()) return false;

    return true;
}

static bool test_func5()
{
    DccPktFunc5 pkt(3);

    for (int f = 5; f <= 8; f++) {
        if (pkt.get_f(f)) return false;
    }

    for (int f = 5; f <= 8; f++) {
        pkt.set_f(f, true);
        if (!pkt.get_f(f)) return false;
        if (!pkt.check_xor()) return false;
        pkt.set_f(f, false);
    }

    return true;
}

static bool test_func9()
{
    DccPktFunc9 pkt(3);

    for (int f = 9; f <= 12; f++) {
        if (pkt.get_f(f)) return false;
    }

    for (int f = 9; f <= 12; f++) {
        pkt.set_f(f, true);
        if (!pkt.get_f(f)) return false;
        if (!pkt.check_xor()) return false;
        pkt.set_f(f, false);
    }

    return true;
}

static bool test_func13()
{
    DccPktFunc13 pkt(3);

    for (int f = 13; f <= 20; f++) {
        if (pkt.get_f(f)) return false;
    }

    for (int f = 13; f <= 20; f++) {
        pkt.set_f(f, true);
        if (!pkt.get_f(f)) return false;
        if (!pkt.check_xor()) return false;
        pkt.set_f(f, false);
    }

    return true;
}

static bool test_func21()
{
    DccPktFunc21 pkt(3);

    for (int f = 21; f <= 28; f++) {
        if (pkt.get_f(f)) return false;
    }

    for (int f = 21; f <= 28; f++) {
        pkt.set_f(f, true);
        if (!pkt.get_f(f)) return false;
        if (!pkt.check_xor()) return false;
        pkt.set_f(f, false);
    }

    return true;
}

static bool test_func29()
{
    DccPktFunc29 pkt(3);

    for (int f = 29; f <= 31; f++) {
        pkt.set_f(f, true);
        if (!pkt.get_f(f)) return false;
        if (!pkt.check_xor()) return false;
        pkt.set_f(f, false);
    }

    return true;
}

// --- CV packets ---

static bool test_ops_read_cv()
{
    DccPktReadCv pkt(3, 1);
    if (!pkt.check_xor()) return false;
    if (pkt.get_type() != DccPkt::OpsRead1Cv) return false;

    // change CV number
    pkt.set_cv(29);
    if (!pkt.check_xor()) return false;

    // long address
    DccPktReadCv pkt2(200, 1024);
    if (!pkt2.check_xor()) return false;
    if (pkt2.get_address() != 200) return false;

    return true;
}

static bool test_ops_write_cv()
{
    DccPktWriteCv pkt(3, 1, 0x55);
    if (!pkt.check_xor()) return false;
    if (pkt.get_type() != DccPkt::OpsWriteCv) return false;

    DccPktWriteCv pkt2(3, 29, 6);
    if (!pkt2.check_xor()) return false;

    return true;
}

static bool test_ops_write_bit()
{
    DccPktWriteBit pkt(3, 1, 0, 1);
    if (!pkt.check_xor()) return false;
    if (pkt.get_type() != DccPkt::OpsWriteBit) return false;

    DccPktWriteBit pkt2(3, 29, 5, 0);
    if (!pkt2.check_xor()) return false;

    return true;
}

static bool test_svc_write_cv()
{
    DccPktSvcWriteCv pkt(1, 0x03);
    if (!pkt.check_xor()) return false;
    if (pkt.msg_len() != 4) return false;

    return true;
}

static bool test_svc_write_bit()
{
    DccPktSvcWriteBit pkt(1, 3, 1);
    if (!pkt.check_xor()) return false;
    if (pkt.msg_len() != 4) return false;

    return true;
}

// --- decode_type ---

static bool test_decode_type()
{
    // Idle
    DccPktIdle idle;
    uint8_t idle_msg[3] = {idle.data(0), idle.data(1), idle.data(2)};
    if (DccPkt::decode_type(idle_msg, 3) != DccPkt::Idle) return false;

    // Reset
    DccPktReset reset;
    uint8_t reset_msg[3] = {reset.data(0), reset.data(1), reset.data(2)};
    if (DccPkt::decode_type(reset_msg, 3) != DccPkt::Reset) return false;

    // Speed128 short address
    DccPktSpeed128 spd(3, 50);
    uint8_t spd_msg[4];
    for (int i = 0; i < 4; i++) spd_msg[i] = spd.data(i);
    if (DccPkt::decode_type(spd_msg, 4) != DccPkt::Speed128) return false;

    // Speed128 long address
    DccPktSpeed128 spd2(200, 50);
    uint8_t spd2_msg[5];
    for (int i = 0; i < 5; i++) spd2_msg[i] = spd2.data(i);
    if (DccPkt::decode_type(spd2_msg, 5) != DccPkt::Speed128) return false;

    // Func0
    DccPktFunc0 f0(3);
    f0.set_f(0, true);
    uint8_t f0_msg[3];
    for (int i = 0; i < 3; i++) f0_msg[i] = f0.data(i);
    if (DccPkt::decode_type(f0_msg, 3) != DccPkt::Func0) return false;

    // Func5
    DccPktFunc5 f5(3);
    uint8_t f5_msg[3];
    for (int i = 0; i < 3; i++) f5_msg[i] = f5.data(i);
    if (DccPkt::decode_type(f5_msg, 3) != DccPkt::Func5) return false;

    // Func9
    DccPktFunc9 f9(3);
    uint8_t f9_msg[3];
    for (int i = 0; i < 3; i++) f9_msg[i] = f9.data(i);
    if (DccPkt::decode_type(f9_msg, 3) != DccPkt::Func9) return false;

    // Func13
    DccPktFunc13 f13(3);
    uint8_t f13_msg[4];
    for (int i = 0; i < 4; i++) f13_msg[i] = f13.data(i);
    if (DccPkt::decode_type(f13_msg, 4) != DccPkt::Func13) return false;

    return true;
}

// --- check_xor ---

static bool test_check_xor()
{
    DccPktIdle idle;
    uint8_t msg[3];
    for (int i = 0; i < 3; i++) msg[i] = idle.data(i);

    // valid
    if (!DccPkt::check_xor(msg, 3)) return false;

    // corrupt one byte
    msg[1] ^= 0x01;
    if (DccPkt::check_xor(msg, 3)) return false;

    return true;
}

// --- DccPktSetAdrs ---

static bool test_set_adrs()
{
    DccPktSetAdrs pkt(3, 100);
    if (!pkt.check_xor()) return false;
    if (pkt.get_type() != DccPkt::OpsWriteAdrs) return false;
    if (pkt.get_address() != 3) return false;

    // long source address
    DccPktSetAdrs pkt2(200, 5000);
    if (!pkt2.check_xor()) return false;
    if (pkt2.get_address() != 200) return false;

    return true;
}

// --- show() output ---

static bool test_show_output()
{
    char buf[80];

    // Idle shows "idle"
    DccPktIdle idle;
    idle.show(buf, sizeof(buf));
    if (strstr(buf, "idle") == nullptr) return false;

    // Speed shows address and speed
    DccPktSpeed128 spd(3, 50);
    spd.show(buf, sizeof(buf));
    if (strstr(buf, "3 ") == nullptr) return false;
    if (strstr(buf, "50") == nullptr) return false;

    // Reset shows "reset"
    DccPktReset rst;
    rst.show(buf, sizeof(buf));
    if (strstr(buf, "reset") == nullptr) return false;

    return true;
}

// --- decode_speed_128 and decode_func ---

static bool test_decode_speed_128()
{
    DccPktSpeed128 pkt(3, 75);
    int speed = 0;
    if (!pkt.decode_speed_128(speed)) return false;
    if (speed != 75) return false;

    DccPktSpeed128 pkt2(3, -42);
    if (!pkt2.decode_speed_128(speed)) return false;
    if (speed != -42) return false;

    return true;
}

static bool test_decode_func_groups()
{
    // F0-F4
    DccPktFunc0 pkt0(3);
    pkt0.set_f(0, true);
    pkt0.set_f(3, true);
    int f[8];
    if (!pkt0.decode_func_0(f)) return false;
    if (f[0] != 1) return false; // f0
    if (f[3] != 1) return false; // f3
    if (f[1] != 0) return false; // f1

    // F5-F8
    DccPktFunc5 pkt5(3);
    pkt5.set_f(6, true);
    if (!pkt5.decode_func_5(f)) return false;
    if (f[1] != 1) return false; // f6 is index 1 (f5=0, f6=1, ...)

    // F9-F12
    DccPktFunc9 pkt9(3);
    pkt9.set_f(10, true);
    if (!pkt9.decode_func_9(f)) return false;
    if (f[1] != 1) return false; // f10 is index 1

    // F13-F20
    DccPktFunc13 pkt13(3);
    pkt13.set_f(15, true);
    if (!pkt13.decode_func_13(f)) return false;
    if (f[2] != 1) return false; // f15 is index 2

    return true;
}

// --- Function with long address ---

static bool test_func_long_address()
{
    DccPktFunc0 pkt(200);
    if (pkt.get_address() != 200) return false;
    if (pkt.msg_len() != 4) return false; // 2 addr + inst + xor

    pkt.set_f(0, true);
    if (!pkt.get_f(0)) return false;
    if (!pkt.check_xor()) return false;

    // is_type should match
    uint8_t msg[4];
    for (int i = 0; i < 4; i++) msg[i] = pkt.data(i);
    if (!DccPktFunc0::is_type(msg, 4)) return false;

    return true;
}

// --- Speed128::is_type ---

static bool test_speed128_is_type()
{
    DccPktSpeed128 pkt(3, 50);
    uint8_t msg[4];
    for (int i = 0; i < 4; i++) msg[i] = pkt.data(i);
    if (!DccPktSpeed128::is_type(msg, 4)) return false;

    // long address
    DccPktSpeed128 pkt2(200, 50);
    uint8_t msg2[5];
    for (int i = 0; i < 5; i++) msg2[i] = pkt2.data(i);
    if (!DccPktSpeed128::is_type(msg2, 5)) return false;

    // not a speed packet
    DccPktFunc0 f0(3);
    uint8_t msg3[3];
    for (int i = 0; i < 3; i++) msg3[i] = f0.data(i);
    if (DccPktSpeed128::is_type(msg3, 3)) return false;

    return true;
}

extern const Test tests_dcc_pkt[] = {
    {"idle_packet", test_idle_packet},
    {"reset_packet", test_reset_packet},
    {"short_address", test_short_address},
    {"long_address", test_long_address},
    {"speed128_forward", test_speed128_forward},
    {"speed128_reverse", test_speed128_reverse},
    {"speed128_set_get", test_speed128_set_get},
    {"func0", test_func0},
    {"func5", test_func5},
    {"func9", test_func9},
    {"func13", test_func13},
    {"func21", test_func21},
    {"func29", test_func29},
    {"ops_read_cv", test_ops_read_cv},
    {"ops_write_cv", test_ops_write_cv},
    {"ops_write_bit", test_ops_write_bit},
    {"svc_write_cv", test_svc_write_cv},
    {"svc_write_bit", test_svc_write_bit},
    {"decode_type", test_decode_type},
    {"check_xor", test_check_xor},
    {"set_adrs", test_set_adrs},
    {"show_output", test_show_output},
    {"decode_speed_128", test_decode_speed_128},
    {"decode_func_groups", test_decode_func_groups},
    {"func_long_address", test_func_long_address},
    {"speed128_is_type", test_speed128_is_type},
};

extern const int tests_dcc_pkt_cnt = sizeof(tests_dcc_pkt) / sizeof(tests_dcc_pkt[0]);
