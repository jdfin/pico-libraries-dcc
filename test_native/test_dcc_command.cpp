#include <cstdio>
#include <cstdint>
#include <cstring>

#include "dcc_adc.h"
#include "dcc_command.h"
#include "dcc_pkt.h"
#include "dcc_pkt2.h"
#include "dcc_spec.h"
#include "test.h"

// Helper: get packet type by decoding bytes (works for ops packets)
static DccPkt::PktType pkt_type(DccPkt2 &pkt)
{
    uint8_t buf[8];
    int len = pkt.len();
    if (len > (int)sizeof(buf)) len = (int)sizeof(buf);
    for (int i = 0; i < len; i++) buf[i] = pkt.data(i);
    return DccPkt::decode_type(buf, len);
}

// Helper: check if packet is a reset packet (00 00 00)
static bool is_reset(DccPkt2 &pkt)
{
    return pkt.len() == 3 && pkt.data(0) == 0x00 && pkt.data(1) == 0x00;
}

// Helper: check if packet is NOT a reset (i.e. a command packet)
static bool is_not_reset(DccPkt2 &pkt)
{
    return !is_reset(pkt);
}

// Stub ADC control functions (defined in stub_dcc_adc.cpp)
extern void stub_adc_set_short_avg_ma(uint16_t val);
extern void stub_adc_set_long_avg_ma(uint16_t val);
extern void stub_adc_set_loop_result(bool val);

// Helper: create a DccCommand with stub ADC
// sig_gpio=0, pwr_gpio=1 satisfies DccBitstream assertion (same slice, different channels)
struct CmdFixture {
    DccAdc adc;
    DccCommand cmd;

    CmdFixture() : adc(26), cmd(0, 1, -1, adc) {}

    ~CmdFixture()
    {
        // Reset stub state
        stub_adc_set_short_avg_ma(0);
        stub_adc_set_long_avg_ma(100);
        stub_adc_set_loop_result(false);
    }
};

// Helper: pump the svc state machine by calling get_packet n times
static void pump(DccCommand &cmd, int n)
{
    DccPkt2 pkt;
    for (int i = 0; i < n; i++)
        cmd.get_packet(pkt);
}

// Helper: inject an ACK via the stub ADC
static void inject_ack(DccCommand &cmd)
{
    stub_adc_set_loop_result(true);
    stub_adc_set_short_avg_ma(200);
    cmd.loop();
    stub_adc_set_loop_result(false);
    stub_adc_set_short_avg_ma(0);
}

// --- Loco management tests ---

static bool test_find_loco_empty()
{
    CmdFixture f;
    if (f.cmd.find_loco(3) != nullptr) return false;
    if (f.cmd.find_loco(1) != nullptr) return false;
    return true;
}

static bool test_create_find_loco()
{
    CmdFixture f;

    DccLoco *loco = f.cmd.create_loco(3);
    if (loco == nullptr) return false;
    if (loco->get_address() != 3) return false;
    if (f.cmd.find_loco(3) != loco) return false;
    if (f.cmd.find_loco(4) != nullptr) return false;
    return true;
}

static bool test_create_multiple_locos()
{
    CmdFixture f;

    DccLoco *l3 = f.cmd.create_loco(3);
    DccLoco *l5 = f.cmd.create_loco(5);
    DccLoco *l1 = f.cmd.create_loco(1);

    if (l3 == nullptr || l5 == nullptr || l1 == nullptr) return false;
    if (l3->get_address() != 3) return false;
    if (l5->get_address() != 5) return false;
    if (l1->get_address() != 1) return false;

    if (f.cmd.find_loco(3) != l3) return false;
    if (f.cmd.find_loco(5) != l5) return false;
    if (f.cmd.find_loco(1) != l1) return false;

    return true;
}

static bool test_create_duplicate_loco()
{
    CmdFixture f;

    DccLoco *l1 = f.cmd.create_loco(3);
    DccLoco *l2 = f.cmd.create_loco(3);
    if (l1 != l2) return false;
    return true;
}

static bool test_delete_loco()
{
    CmdFixture f;

    f.cmd.create_loco(3);
    DccLoco *l5 = f.cmd.create_loco(5);

    f.cmd.delete_loco(3);
    if (f.cmd.find_loco(3) != nullptr) return false;
    if (f.cmd.find_loco(5) != l5) return false;

    return true;
}

static bool test_create_invalid_address()
{
    CmdFixture f;
    if (f.cmd.create_loco(0) != nullptr) return false;
    if (f.cmd.create_loco(-1) != nullptr) return false;
    if (f.cmd.create_loco(DccPkt::address_max + 1) != nullptr) return false;
    return true;
}

// --- Ops mode dispatch tests ---

static bool test_ops_idle_no_locos()
{
    CmdFixture f;
    f.cmd.set_mode_ops();

    DccPkt2 pkt;
    f.cmd.get_packet(pkt);
    if (pkt_type(pkt) != DccPkt::Idle) return false;
    return true;
}

static bool test_ops_round_robin()
{
    CmdFixture f;

    f.cmd.create_loco(3);
    f.cmd.create_loco(5);
    f.cmd.set_mode_ops();

    DccPkt2 pkt;

    f.cmd.get_packet(pkt);
    DccLoco *first = pkt.get_loco();
    if (first == nullptr) return false;

    f.cmd.get_packet(pkt);
    DccLoco *second = pkt.get_loco();
    if (second == nullptr) return false;
    if (first == second) return false;

    f.cmd.get_packet(pkt);
    DccLoco *third = pkt.get_loco();
    if (third != first) return false;

    return true;
}

// --- Service mode write tests ---

// Verify write CV sequence: 20 resets + 5 commands + 5 resets, no ack → error
static bool test_svc_write_cv_no_ack()
{
    CmdFixture f;

    stub_adc_set_loop_result(false);
    stub_adc_set_long_avg_ma(100);

    f.cmd.write_cv(1, 0x42);
    if (f.cmd.mode() != DccCommand::Mode::SVC) return false;

    DccPkt2 pkt;

    // Phase 1: svc_reset1_cnt (20) reset packets
    for (int i = 0; i < DccSpec::svc_reset1_cnt; i++) {
        f.cmd.get_packet(pkt);
        if (!is_reset(pkt)) return false;
    }

    // Phase 2: svc_command_cnt (5) command packets (not reset)
    for (int i = 0; i < DccSpec::svc_command_cnt; i++) {
        f.cmd.get_packet(pkt);
        if (!is_not_reset(pkt)) return false;
    }

    // Phase 3: svc_reset2_cnt (5) reset packets
    for (int i = 0; i < DccSpec::svc_reset2_cnt; i++) {
        f.cmd.get_packet(pkt);
        if (!is_reset(pkt)) return false;
    }

    // One more call to finalize (transitions to OFF)
    f.cmd.get_packet(pkt);

    bool result;
    if (!f.cmd.svc_done(result)) return false;
    if (result != false) return false;
    if (f.cmd.mode() != DccCommand::Mode::OFF) return false;

    return true;
}

// Verify write CV with ack → early termination, success
static bool test_svc_write_cv_with_ack()
{
    CmdFixture f;

    stub_adc_set_loop_result(false);
    stub_adc_set_long_avg_ma(100);

    f.cmd.write_cv(1, 0x42);

    // Phase 1: 20 resets
    pump(f.cmd, DccSpec::svc_reset1_cnt);

    // After resets, ack is armed (threshold = long_avg + 60 = 160)
    // Send first write command
    DccPkt2 pkt;
    f.cmd.get_packet(pkt);
    if (!is_not_reset(pkt)) return false;

    // Inject ack
    inject_ack(f.cmd);

    // Next get_packet should see ack and start finalization
    f.cmd.get_packet(pkt);

    bool result;
    if (!f.cmd.svc_done(result)) return false;
    if (result != true) return false;

    return true;
}

// Verify write bit sequence: 20 resets + 5 commands + 5 resets, no ack → error
static bool test_svc_write_bit_no_ack()
{
    CmdFixture f;

    stub_adc_set_loop_result(false);
    stub_adc_set_long_avg_ma(100);

    f.cmd.write_bit(1, 3, 1);

    DccPkt2 pkt;

    // Phase 1: 20 resets
    for (int i = 0; i < DccSpec::svc_reset1_cnt; i++) {
        f.cmd.get_packet(pkt);
        if (!is_reset(pkt)) return false;
    }

    // Phase 2: 5 write-bit commands
    for (int i = 0; i < DccSpec::svc_command_cnt; i++) {
        f.cmd.get_packet(pkt);
        if (!is_not_reset(pkt)) return false;
    }

    // Phase 3: 5 resets
    for (int i = 0; i < DccSpec::svc_reset2_cnt; i++) {
        f.cmd.get_packet(pkt);
        if (!is_reset(pkt)) return false;
    }

    // Finalize
    f.cmd.get_packet(pkt);

    bool result;
    if (!f.cmd.svc_done(result)) return false;
    if (result != false) return false;

    return true;
}

// --- Service mode read tests ---

// Read CV with no acks: all bits read as 0, byte verify fails → error
static bool test_svc_read_cv_all_zeros()
{
    CmdFixture f;

    stub_adc_set_loop_result(false);
    stub_adc_set_long_avg_ma(100);

    f.cmd.read_cv(1);

    DccPkt2 pkt;

    // Phase 1: 20 resets
    for (int i = 0; i < DccSpec::svc_reset1_cnt; i++) {
        f.cmd.get_packet(pkt);
        if (!is_reset(pkt)) return false;
    }

    // For each of 8 bits (7 down to 0):
    //   5 verify-bit commands + 5 resets
    for (int bit = 7; bit >= 0; bit--) {
        for (int i = 0; i < DccSpec::svc_command_cnt; i++) {
            f.cmd.get_packet(pkt);
            if (!is_not_reset(pkt)) return false;
        }
        for (int i = 0; i < DccSpec::svc_reset2_cnt; i++) {
            f.cmd.get_packet(pkt);
            if (!is_reset(pkt)) return false;
        }
    }

    // Final byte verify: 5 commands + 5 resets
    for (int i = 0; i < DccSpec::svc_command_cnt; i++) {
        f.cmd.get_packet(pkt);
        if (!is_not_reset(pkt)) return false;
    }
    for (int i = 0; i < DccSpec::svc_reset2_cnt; i++) {
        f.cmd.get_packet(pkt);
        if (!is_reset(pkt)) return false;
    }

    // Finalize
    f.cmd.get_packet(pkt);

    bool result;
    uint8_t val;
    if (!f.cmd.svc_done(result, val)) return false;
    if (result != false) return false;
    if (val != 0x00) return false;

    return true;
}

// Read CV 0xA5: inject acks for bits 7,5,2,0 and byte verify → success
static bool test_svc_read_cv_with_acks()
{
    CmdFixture f;

    stub_adc_set_loop_result(false);
    stub_adc_set_long_avg_ma(100);

    f.cmd.read_cv(1);

    // Phase 1: 20 resets
    pump(f.cmd, DccSpec::svc_reset1_cnt);

    // Simulate reading 0xA5 = 10100101
    uint8_t expected = 0xA5;
    DccPkt2 pkt;

    for (int bit = 7; bit >= 0; bit--) {
        bool bit_is_one = (expected >> bit) & 1;

        for (int i = 0; i < DccSpec::svc_command_cnt; i++) {
            f.cmd.get_packet(pkt);

            // Inject ack during first verify-bit command for 1-bits
            if (bit_is_one && i == 0) {
                inject_ack(f.cmd);
            }
        }

        // Resets after bit verify
        pump(f.cmd, DccSpec::svc_reset2_cnt);
    }

    // Final byte verify: inject ack on first command
    f.cmd.get_packet(pkt);
    inject_ack(f.cmd);

    // Ack detected → early termination
    f.cmd.get_packet(pkt);

    bool result;
    uint8_t val;
    if (!f.cmd.svc_done(result, val)) return false;
    if (result != true) return false;
    if (val != expected) return false;

    return true;
}

// Read single bit: first tries 0 (no ack), then tries 1 (ack) → success, val=1
static bool test_svc_read_bit()
{
    CmdFixture f;

    stub_adc_set_loop_result(false);
    stub_adc_set_long_avg_ma(100);

    f.cmd.read_bit(1, 3);

    // Phase 1: 20 resets
    pump(f.cmd, DccSpec::svc_reset1_cnt);

    DccPkt2 pkt;

    // First tries verify bit=0: 5 commands + 5 resets (no ack)
    pump(f.cmd, DccSpec::svc_command_cnt);
    pump(f.cmd, DccSpec::svc_reset2_cnt);

    // No ack for 0, now tries verify bit=1
    // Inject ack on first command
    f.cmd.get_packet(pkt);
    inject_ack(f.cmd);

    // Next call triggers finalization
    f.cmd.get_packet(pkt);

    bool result;
    uint8_t val;
    if (!f.cmd.svc_done(result, val)) return false;
    if (result != true) return false;
    if (val != 1) return false;

    return true;
}

// --- Mode tests ---

static bool test_mode_transitions()
{
    CmdFixture f;
    if (f.cmd.mode() != DccCommand::Mode::OFF) return false;

    f.cmd.set_mode_ops();
    if (f.cmd.mode() != DccCommand::Mode::OPS) return false;

    f.cmd.set_mode_off();
    if (f.cmd.mode() != DccCommand::Mode::OFF) return false;

    return true;
}

extern const Test tests_dcc_command[] = {
    {"cmd_find_loco_empty", test_find_loco_empty},
    {"cmd_create_find_loco", test_create_find_loco},
    {"cmd_create_multiple_locos", test_create_multiple_locos},
    {"cmd_create_duplicate_loco", test_create_duplicate_loco},
    {"cmd_delete_loco", test_delete_loco},
    {"cmd_create_invalid_address", test_create_invalid_address},
    {"cmd_ops_idle_no_locos", test_ops_idle_no_locos},
    {"cmd_ops_round_robin", test_ops_round_robin},
    {"cmd_svc_write_cv_no_ack", test_svc_write_cv_no_ack},
    {"cmd_svc_write_cv_with_ack", test_svc_write_cv_with_ack},
    {"cmd_svc_write_bit_no_ack", test_svc_write_bit_no_ack},
    {"cmd_svc_read_cv_all_zeros", test_svc_read_cv_all_zeros},
    {"cmd_svc_read_cv_with_acks", test_svc_read_cv_with_acks},
    {"cmd_svc_read_bit", test_svc_read_bit},
    {"cmd_mode_transitions", test_mode_transitions},
};

extern const int tests_dcc_command_cnt =
    sizeof(tests_dcc_command) / sizeof(tests_dcc_command[0]);
