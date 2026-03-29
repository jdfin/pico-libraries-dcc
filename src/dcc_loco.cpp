#include "dcc_loco.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "buf_log.h"
#include "dcc_pkt.h"
#include "hardware/timer.h"
#include "railcom_msg.h"
#include "railcom_spec.h"

DccLoco::DccLoco(int address) :
    _seq(0),
    _pkt_last(nullptr),
    _read_cv_cnt(0),
    _write_cv_cnt(0),
    _write_bit_cnt(0),
    _set_adrs_cnt(0),
    _ops_cv_done(false),
    _ops_cv_status(false),
    _ops_cv_val(0),
    _ops_cv_cb(nullptr),
    _ops_cv_next_cb(nullptr),
    _ops_cv_lockout(0),
    _rc_speed(0),
    _rc_speed_us(UINT64_MAX),
    _show_rc_speed(false),
    _rc_speed_cb(nullptr)
{
    set_address(address);
}

DccLoco::~DccLoco()
{
}

int DccLoco::get_address() const
{
    return _pkt_speed.get_address();
}

void DccLoco::set_address(int address)
{
    _pkt_speed.set_address(address);
    _pkt_func_0.set_address(address);
    _pkt_func_5.set_address(address);
    _pkt_func_9.set_address(address);
    _pkt_func_13.set_address(address);
#if (DCC_FUNC_MAX >= 21)
    _pkt_func_21.set_address(address);
#endif
#if (DCC_FUNC_MAX >= 29)
    _pkt_func_29.set_address(address);
#endif
#if (DCC_FUNC_MAX >= 37)
    _pkt_func_37.set_address(address);
#endif
#if (DCC_FUNC_MAX >= 45)
    _pkt_func_45.set_address(address);
#endif
#if (DCC_FUNC_MAX >= 53)
    _pkt_func_53.set_address(address);
#endif
#if (DCC_FUNC_MAX >= 61)
    _pkt_func_61.set_address(address);
#endif
    _pkt_read_cv.set_address(address);
    _pkt_write_cv.set_address(address);
    _pkt_write_bit.set_address(address);
    _pkt_set_adrs.set_address(address);
    _seq = 0;
}

int DccLoco::get_speed() const
{
    return _pkt_speed.get_speed();
}

void DccLoco::set_speed(int speed)
{
    _pkt_speed.set_speed(speed);
    _seq &= ~1; // back up one if a function packet is next
}

bool DccLoco::get_function(int num) const
{
    assert(DccPkt::function_min <= num && num <= DccPkt::function_max);

    if (num <= 4) {
        return _pkt_func_0.get_f(num);
    } else if (num <= 8) {
        return _pkt_func_5.get_f(num);
    } else if (num <= 12) {
        return _pkt_func_9.get_f(num);
    } else if (num <= 20) {
        return _pkt_func_13.get_f(num);
#if (DCC_FUNC_MAX >= 21)
    } else if (num <= 28) {
        return _pkt_func_21.get_f(num);
#endif
#if (DCC_FUNC_MAX >= 29)
    } else if (num <= 36) {
        return _pkt_func_29.get_f(num);
#endif
#if (DCC_FUNC_MAX >= 37)
    } else if (num <= 44) {
        return _pkt_func_37.get_f(num);
#endif
#if (DCC_FUNC_MAX >= 45)
    } else if (num <= 52) {
        return _pkt_func_45.get_f(num);
#endif
#if (DCC_FUNC_MAX >= 53)
    } else if (num <= 60) {
        return _pkt_func_53.get_f(num);
#endif
#if (DCC_FUNC_MAX >= 61)
    } else if (num <= 68) {
        return _pkt_func_61.get_f(num);
#endif
    } else {
        assert(false);
        return false;
    }
}

void DccLoco::set_function(int num, bool on)
{
    assert(DccPkt::function_min <= num && num <= DccPkt::function_max);

    if (num <= 4) {
        _pkt_func_0.set_f(num, on);
        _seq = 1;
    } else if (num <= 8) {
        _pkt_func_5.set_f(num, on);
        _seq = 3;
    } else if (num <= 12) {
        _pkt_func_9.set_f(num, on);
        _seq = 5;
    } else if (num <= 20) {
        _pkt_func_13.set_f(num, on);
        _seq = 7;
#if (DCC_FUNC_MAX >= 21)
    } else if (num <= 28) {
        _pkt_func_21.set_f(num, on);
        _seq = 9;
#endif
#if (DCC_FUNC_MAX >= 29)
    } else if (num <= 36) {
        _pkt_func_29.set_f(num, on);
        _seq = 11;
#endif
#if (DCC_FUNC_MAX >= 37)
    } else if (num <= 44) {
        _pkt_func_37.set_f(num, on);
        _seq = 13;
#endif
#if (DCC_FUNC_MAX >= 45)
    } else if (num <= 52) {
        _pkt_func_45.set_f(num, on);
        _seq = 15;
#endif
#if (DCC_FUNC_MAX >= 53)
    } else if (num <= 60) {
        _pkt_func_53.set_f(num, on);
        _seq = 17;
#endif
#if (DCC_FUNC_MAX >= 61)
    } else if (num <= 68) {
        _pkt_func_61.set_f(num, on);
        _seq = 19;
#endif
    } else {
        assert(false);
    }
}

// ops mode cv access

void DccLoco::read_cv(int cv_num, OpsCvCb *cb)
{
    _pkt_read_cv.set_cv(cv_num);
    _ops_cv_done = false;
    _ops_cv_status = false;
    // +1 because when it decrements to zero it's an error
    _read_cv_cnt = read_cv_send_cnt + 1;
    _ops_cv_next_cb = cb;
}

void DccLoco::write_cv(int cv_num, uint8_t cv_val, OpsCvCb *cb)
{
    _pkt_write_cv.set_cv(cv_num, cv_val);
    _ops_cv_done = false;
    _ops_cv_status = false;
    _write_cv_cnt = write_cv_send_cnt;
    _ops_cv_next_cb = cb;
}

void DccLoco::write_bit(int cv_num, int bit_num, int bit_val, OpsCvCb *cb)
{
    _pkt_write_bit.set_cv_bit(cv_num, bit_num, bit_val);
    _ops_cv_done = false;
    _ops_cv_status = false;
    _write_bit_cnt = write_bit_send_cnt;
    _ops_cv_next_cb = cb;
}

void DccLoco::set_adrs_new(int adrs_new, OpsCvCb *cb)
{
    _pkt_set_adrs.set_adrs_new(adrs_new);
    _ops_cv_done = false;
    _ops_cv_status = false;
    _set_adrs_cnt = set_adrs_send_cnt;
    _ops_cv_next_cb = cb;
}

bool DccLoco::ops_done(bool &result, uint8_t &value)
{
    if (!_ops_cv_done)
        return false;

    result = _ops_cv_status;
    value = _ops_cv_val;

    return true;
}

// A counter counts up as we send packets. If the counter is even, we send
// the speed. If the counter is odd, we send the next function packet. After
// sending the last function packet, we start over at zero (speed).
//
//  0. Speed     1. F0-F4
//  2. Speed     3. F5-F8
//  4. Speed     5. F9-F12
//  6. Speed     7. F13-F20
//  8. Speed     9. F21-F28
// 10. Speed    11. F29-F36
// (It is common to wrap back to zero here,
//  but we can send more functions if so configured.)
// 12. Speed    13. F37-F44
// 14. Speed    15. F45-F52
// 16. Speed    17. F53-F60
// 18. Speed    19. F61-F68
//
// CV access: The spec says the decoder must get two CV access packets back
// to back (9.2.1, 2.3.7.3). We send more than that (typically 5) to allow
// for errors. When a response is received via railcom, we stop sending.
// However, because the responses are delayed by one packet, and we can't
// tell which response packet goes with which request packet, we have to
// space out the requests a little so we don't get them confused. Further,
// when a CV write is done, the decoder (ESU LokPilot) seems to stop sending
// railcom responses for a few packets starting several packets after the
// write is acknowledged, so we wait long enough for that time to go by too.
//
// Heuristics:
// (1) To avoid getting confused about which response goes with which request,
// we always wait at least 'ops_cv_read_lockout' packets after a read response
// is received before sending another read request.
// (2) To avoid that no-response time after cv writes, we wait at least
// 'ops_cv_write_lockout' packets after a write response is received before
// sending another write request.
// 'ops_cv_lockout' set from one of those and counts down after a cv access.
//
DccPkt DccLoco::next_packet()
{
    assert(0 <= _seq && _seq < seq_max);

    if (_ops_cv_lockout == 0) {
        // can send an ops cv packet if needed

        if (_ops_cv_next_cb != nullptr) {
            _ops_cv_cb = _ops_cv_next_cb;
            _ops_cv_next_cb = nullptr;
        }

        if (_read_cv_cnt > 0) {
            _read_cv_cnt--;
            if (_read_cv_cnt == 0) {
                // No response. Since CV read requires railcom, this is an error.
                _ops_cv_done = true;
                _ops_cv_status = false;
                _ops_cv_val = 0x00; // arbitrary, seems better to set it
                if (_ops_cv_cb != nullptr) {
                    _ops_cv_cb(this, false, 0);
                    _ops_cv_cb = nullptr;
                }
                _ops_cv_lockout = ops_cv_read_lockout;
#if 0
                char *b = BufLog::write_line_get();
                if (b != nullptr) {
                    char *e = b + BufLog::line_len;
                    b += snprintf(b, e - b, "%d: lockout=%d", __LINE__,
                                  _ops_cv_lockout);
                    BufLog::write_line_put();
                }
#endif
                // continue on below to return a different packet
            } else {
                _pkt_last = &_pkt_read_cv;
                return _pkt_read_cv;
            }
        }

        if (_write_cv_cnt > 0) {
            _write_cv_cnt--;
            _pkt_last = &_pkt_write_cv;
            return _pkt_write_cv;
        }

        if (_write_bit_cnt > 0) {
            _write_bit_cnt--;
            _pkt_last = &_pkt_write_bit;
            return _pkt_write_bit;
        }

        if (_set_adrs_cnt > 0) {
            _set_adrs_cnt--;
            _pkt_last = &_pkt_set_adrs;
            return _pkt_set_adrs;
        }

    } else {
        assert(_ops_cv_lockout > 0);
        _ops_cv_lockout--;
#if 0
        char *b = BufLog::write_line_get();
        if (b != nullptr) {
            char *e = b + BufLog::line_len;
            b +=
                snprintf(b, e - b, "%d: lockout=%d", __LINE__, _ops_cv_lockout);
            BufLog::write_line_put();
        }
#endif
    }

    // Either _ops_cv_lockout is zero and there were no ops cv packets to
    // send, or _ops_cv_lockout was nonzero and we're waiting the lockout
    // time. Send the next speed/function packet in the sequence.

    int seq = _seq;

    if (++_seq >= seq_max)
        _seq = 0;

    if ((seq & 1) == 0) { // if _seq even
        _pkt_last = &_pkt_speed;
        return _pkt_speed;
    } else if (seq == 1) {
        _pkt_last = &_pkt_func_0;
        return _pkt_func_0;
    } else if (seq == 3) {
        _pkt_last = &_pkt_func_5;
        return _pkt_func_5;
    } else if (seq == 5) {
        _pkt_last = &_pkt_func_9;
        return _pkt_func_9;
    } else if (seq == 7) {
        _pkt_last = &_pkt_func_13;
        return _pkt_func_13;
#if (DCC_FUNC_MAX >= 21)
    } else if (seq == 9) {
        _pkt_last = &_pkt_func_21;
        return _pkt_func_21;
#endif
#if (DCC_FUNC_MAX >= 29)
    } else if (seq == 11) {
        _pkt_last = &_pkt_func_29;
        return _pkt_func_29;
#endif
#if (DCC_FUNC_MAX >= 37)
    } else if (seq == 13) {
        _pkt_last = &_pkt_func_37;
        return _pkt_func_37;
#endif
#if (DCC_FUNC_MAX >= 45)
    } else if (seq == 15) {
        _pkt_last = &_pkt_func_45;
        return _pkt_func_45;
#endif
#if (DCC_FUNC_MAX >= 53)
    } else if (seq == 17) {
        _pkt_last = &_pkt_func_53;
        return _pkt_func_53;
#endif
#if (DCC_FUNC_MAX >= 61)
    } else if (seq == 19) {
        _pkt_last = &_pkt_func_61;
        return _pkt_func_61;
#endif
    } else {
        __builtin_unreachable();
    }
}

// This is called (at interrupt level) if any railcom channel2 messages are
// received in the cutout following a DCC message from this loco.

void DccLoco::railcom(const RailComMsg *const msg,
                      int msg_cnt) // called in interrupt context
{
    constexpr int verbosity = 0;

    // verbosity 9: print all dcc sent and railcom received
    // verbosity 1: print only railcom pom received

    if constexpr (verbosity >= 9) {

        char *b = BufLog::write_line_get();
        if (b == nullptr)
            return;
        char *e = b + BufLog::line_len;

        // show DCC packet sent
        b += snprintf(b, e - b, "{");
        if (_pkt_last != nullptr) {
            _pkt_last->show(b, e - b);
            b += strlen(b);
        }
        b += snprintf(b, e - b, "}");

        // show railcom packet received
        b += snprintf(b, e - b, " {");
        if (msg_cnt == 0) {
            b += snprintf(b, e - b, " no data");
        } else {
            for (int i = 0; i < msg_cnt; i++) {
                b += snprintf(b, e - b, " ");
                b += msg[i].show(b, e - b);
            }
        }
        b += snprintf(b, e - b, "}");

        BufLog::write_line_put();

    } else if constexpr (verbosity >= 1) {

        char *b = nullptr; // get a log line only when needed
        char *e = nullptr;

        for (int i = 0; i < msg_cnt; i++) {
            if (msg[i].id == RailComMsg::MsgId::pom) {
                if (b == nullptr) {
                    b = BufLog::write_line_get();
                    if (b == nullptr)
                        return;
                    e = b + BufLog::line_len;
                } else {
                    // if there are two, put a space between them
                    b += snprintf(b, e - b, " ");
                }
                b += msg[i].show(b, e - b);
            }
        }

        if (b != nullptr)
            BufLog::write_line_put();
    }

    // process messages received

    for (int i = 0; i < msg_cnt; i++) {
        if (msg[i].id == RailComMsg::MsgId::pom) {
            if (_ops_cv_lockout == 0) {
                if (_read_cv_cnt > 0) {
                    assert(_write_cv_cnt == 0 && _write_bit_cnt == 0 &&
                           _set_adrs_cnt == 0);
                    _ops_cv_done = true;
                    _ops_cv_status = true;
                    _ops_cv_val = msg[i].pom.val;
                    _read_cv_cnt = 0;
                    _ops_cv_lockout = ops_cv_read_lockout;
#if 0
                    char *b = BufLog::write_line_get();
                    if (b != nullptr) {
                        char *e = b + BufLog::line_len;
                        b += snprintf(b, e - b, "%d: lockout=%d", __LINE__,
                                      _ops_cv_lockout);
                        BufLog::write_line_put();
                    }
#endif
                    if (_ops_cv_cb != nullptr) {
                        _ops_cv_cb(this, true, msg[i].pom.val);
                        _ops_cv_cb = nullptr;
                    }
                } else if (_write_cv_cnt > 0) {
                    assert(_write_bit_cnt == 0 && _set_adrs_cnt == 0);
                    _ops_cv_done = true;
                    _ops_cv_status = true;
                    _ops_cv_val = msg[i].pom.val;
                    _write_cv_cnt = 0;
                    _ops_cv_lockout = ops_cv_write_lockout;
#if 0
                    char *b = BufLog::write_line_get();
                    if (b != nullptr) {
                        char *e = b + BufLog::line_len;
                        b += snprintf(b, e - b, "%d: lockout=%d", __LINE__,
                                      _ops_cv_lockout);
                        BufLog::write_line_put();
                    }
#endif
                    if (_ops_cv_cb != nullptr) {
                        _ops_cv_cb(this, true, msg[i].pom.val);
                        _ops_cv_cb = nullptr;
                    }
                } else if (_write_bit_cnt > 0) {
                    assert(_set_adrs_cnt == 0);
                    _ops_cv_done = true;
                    _ops_cv_status = true;
                    _ops_cv_val = msg[i].pom.val;
                    _write_bit_cnt = 0;
                    _ops_cv_lockout = ops_cv_write_lockout;
#if 0
                    char *b = BufLog::write_line_get();
                    if (b != nullptr) {
                        char *e = b + BufLog::line_len;
                        b += snprintf(b, e - b, "%d: lockout=%d", __LINE__,
                                      _ops_cv_lockout);
                        BufLog::write_line_put();
                    }
#endif
                    if (_ops_cv_cb != nullptr) {
                        _ops_cv_cb(this, true, msg[i].pom.val);
                        _ops_cv_cb = nullptr;
                    }
                } else if (_set_adrs_cnt > 0) {
                    _ops_cv_done = true;
                    _ops_cv_status = true;
                    //_ops_cv_val = msg[i].pom.val;
                    _set_adrs_cnt = 0;
                    _ops_cv_lockout = ops_cv_write_lockout;
#if 0
                    char *b = BufLog::write_line_get();
                    if (b != nullptr) {
                        char *e = b + BufLog::line_len;
                        b += snprintf(b, e - b, "%d: lockout=%d", __LINE__,
                                      _ops_cv_lockout);
                        BufLog::write_line_put();
                    }
#endif
                    if (_ops_cv_cb != nullptr) {
                        _ops_cv_cb(this, true, 0); // XXX
                        _ops_cv_cb = nullptr;
                    }
                }
            }
        } else if (msg[i].id == RailComMsg::MsgId::dyn) {
            if (msg[i].dyn.id == RailComSpec::DynId::dyn_speed_1) {
                if (msg[i].dyn.val != _rc_speed) {
                    // loco's self-reported speed has changed
                    _rc_speed = msg[i].dyn.val;
                    _rc_speed_us = time_us_64();
                    if (_rc_speed_cb != nullptr)
                        _rc_speed_cb(this, (_rc_speed_us + 500) / 1000,
                                     _rc_speed);
                    if (_show_rc_speed) {
                        char *b = BufLog::write_line_get();
                        if (b != nullptr) {
                            snprintf(b, BufLog::line_len, "%0.3f speed=%u",
                                     _rc_speed_us / 1000000.0, _rc_speed);
                            BufLog::write_line_put();
                        }
                    }
                }
            }
        }
    }

} // void DccLoco::railcom(const RailComMsg *msg, int msg_cnt)

void DccLoco::show()
{
    char buf[80];
    printf("%s\n", _pkt_speed.show(buf, sizeof(buf)));
    printf("%s\n", _pkt_func_0.show(buf, sizeof(buf)));
    printf("%s\n", _pkt_func_5.show(buf, sizeof(buf)));
    printf("%s\n", _pkt_func_9.show(buf, sizeof(buf)));
    printf("%s\n", _pkt_func_13.show(buf, sizeof(buf)));
#if (DCC_FUNC_MAX >= 21)
    printf("%s\n", _pkt_func_21.show(buf, sizeof(buf)));
#endif
#if (DCC_FUNC_MAX >= 29)
    printf("%s\n", _pkt_func_29.show(buf, sizeof(buf)));
#endif
#if (DCC_FUNC_MAX >= 37)
    printf("%s\n", _pkt_func_37.show(buf, sizeof(buf)));
#endif
#if (DCC_FUNC_MAX >= 45)
    printf("%s\n", _pkt_func_45.show(buf, sizeof(buf)));
#endif
#if (DCC_FUNC_MAX >= 53)
    printf("%s\n", _pkt_func_53.show(buf, sizeof(buf)));
#endif
#if (DCC_FUNC_MAX >= 61)
    printf("%s\n", _pkt_func_61.show(buf, sizeof(buf)));
#endif
}
