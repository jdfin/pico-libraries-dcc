#pragma once

#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>

// ESU LokSound 5 supports F0...F31
#define DCC_FUNC_MAX 31


class DccPkt
{
public:

    DccPkt(const uint8_t *msg = nullptr, int msg_len = 0);

    ~DccPkt()
    {
        memset(_msg, 0, msg_max);
        _msg_len = 0;
    }

    enum PktType {
        Invalid,
        Reset,
        Ccc0, // 2.3.1 Decoder and Consist Control
        Speed128,
        Speed28,
        Func0,
        Func5,
        Func9,
        Func13,
#if (DCC_FUNC_MAX >= 21)
        Func21,
#endif
#if (DCC_FUNC_MAX >= 29)
        Func29,
#endif
#if (DCC_FUNC_MAX >= 37)
        Func37,
#endif
#if (DCC_FUNC_MAX >= 45)
        Func45,
#endif
#if (DCC_FUNC_MAX >= 53)
        Func53,
#endif
#if (DCC_FUNC_MAX >= 61)
        Func61,
#endif
        OpsRead1Cv,
        OpsRead4Cv,
        OpsWriteCv,
        OpsWriteBit,
        SvcWriteCv,
        SvcWriteBit,
        SvcVerifyCv,
        SvcVerifyBit,
        Accessory,
        Reserved,
        Advanced,
        Idle,
        Unimplemented,
    };

    virtual PktType get_type() const
    {
        return Invalid;
    }

    void msg_len(int new_len);

    int msg_len() const
    {
        return _msg_len;
    }

    uint8_t data(int idx) const;

    int get_address() const;
    virtual int set_address(int adrs);
    int get_address_size() const;

    void set_xor();

    bool check_xor() const;
    static bool check_xor(const uint8_t *msg, int msg_len);

    // loco address constraints
    static constexpr int address_min = 1;         // 0 is broadcast
    static constexpr int address_short_max = 127; // 0x7f
    static constexpr int address_max = 10239;     // 0x27ff
    static constexpr int address_inv = INT_MAX;
    static constexpr int address_default = 3;

    static constexpr int speed_min = -127;
    static constexpr int speed_max = 127;
    static constexpr int speed_inv = INT_MAX;

    static constexpr int function_min = 0;
    static constexpr int function_max = DCC_FUNC_MAX;

    static constexpr int cv_num_min = 1;
    static constexpr int cv_num_max = 1024;
    static constexpr int cv_num_inv = INT_MAX;

    // CV values can be specified as int8_t (-127..128) or uint8_t (0..255)
    static constexpr int cv_val_min = -127;
    static constexpr int cv_val_max = 255;
    static constexpr int cv_val_inv = INT_MAX;

    static bool is_svc_direct(const uint8_t *msg, int msg_len);

    char *dump(char *buf, int buf_len) const;
    char *show(char *buf, int buf_len) const;

    // DCC Spec 9.2, section A ("preamble")
    static constexpr int ops_preamble_bits = 14;

    // DCC Spec 9.2.3, section E ("long preamble")
    static constexpr int svc_preamble_bits = 20;

    static PktType decode_type(const uint8_t *msg, int msg_len);

    // dcc_spy uses these
    // not sure what the "correct" way to do these is
    // return true and fill in the parameters if the packet is of the correct
    // type
    bool decode_speed_128(int &speed) const;
    bool decode_func_0(int *f) const;  // f[5] is f0..f4
    bool decode_func_5(int *f) const;  // f[4] is f5..f8
    bool decode_func_9(int *f) const;  // f[4] is f9..f12
    bool decode_func_13(int *f) const; // f[8] is f13..f20
#if (DCC_FUNC_MAX >= 21)
    bool decode_func_21(int *f) const; // f[8] is f21..f28
#endif
#if (DCC_FUNC_MAX >= 29)
    bool decode_func_29(int *f) const; // f[8] is f29..f36
#endif
#if (DCC_FUNC_MAX >= 37)
    bool decode_func_37(int *f) const; // f[8] is f37..f44
#endif
#if (DCC_FUNC_MAX >= 45)
    bool decode_func_45(int *f) const; // f[8] is f45..f52
#endif
#if (DCC_FUNC_MAX >= 53)
    bool decode_func_53(int *f) const; // f[8] is f53..f60
#endif
#if (DCC_FUNC_MAX >= 61)
    bool decode_func_61(int *f) const; // f[8] is f61..f68
#endif

protected:

    static constexpr int msg_max = 8;

    uint8_t _msg[msg_max];

    int _msg_len;

    bool check_len_min(char *&b, char *e, int min_len) const;
    bool check_len_is(char *&b, char *e, int len) const;
    void show_cv_access(char *&b, char *e, uint8_t instr, int idx) const;

    static PktType decode_payload(const uint8_t *pay, int pay_len);

}; // DccPkt

// 2.1 - Address Partitions - Idle Packet
class DccPktIdle : public DccPkt
{
public:

    DccPktIdle();
    virtual PktType get_type() const override
    {
        return Idle;
    }
};

// 2.3.1.1 - Decoder Control
class DccPktReset : public DccPkt
{
public:

    DccPktReset();
    virtual PktType get_type() const override
    {
        return Reset;
    }
};

// 2.3.2.1 - 128 Speed Step Control
class DccPktSpeed128 : public DccPkt
{
public:

    DccPktSpeed128(int adrs = 3, int speed = 0);
    virtual int set_address(int adrs) override;
    int get_speed() const;
    void set_speed(int speed);
    static bool is_type(const uint8_t *msg, int msg_len);
    static uint8_t int_to_dcc(int speed_int);
    static int dcc_to_int(uint8_t speed_dcc);

private:

    void refresh(int adrs, int speed);
    DccPktSpeed128(const uint8_t *msg, int msg_len) : DccPkt(msg, msg_len)
    {
    }
    friend DccPkt create(const uint8_t *msg, int msg_len);
};

// 2.3.4 - Function Group One (F0-F4)
class DccPktFunc0 : public DccPkt
{
public:

    DccPktFunc0(int adrs = 3);
    virtual int set_address(int adrs) override;
    bool get_f(int num) const;
    void set_f(int num, bool on);
    static bool is_type(const uint8_t *msg, int msg_len);

private:

    static constexpr int f_min = 0;
    static constexpr int f_max = 4;
    void refresh(int adrs, uint8_t funcs = 0);
    DccPktFunc0(const uint8_t *msg, int msg_len) : DccPkt(msg, msg_len)
    {
    }
    uint8_t get_funcs() const;
    friend DccPkt create(const uint8_t *msg, int msg_len);
};

// 2.3.5 - Function Group Two (S-bit=1, F5-F8)
class DccPktFunc5 : public DccPkt
{
public:

    DccPktFunc5(int adrs = 3);
    virtual int set_address(int adrs) override;
    bool get_f(int num) const;
    void set_f(int num, bool on);
    static bool is_type(const uint8_t *msg, int msg_len);

private:

    static constexpr int f_min = 5;
    static constexpr int f_max = 8;
    void refresh(int adrs, uint8_t funcs = 0);
    DccPktFunc5(const uint8_t *msg, int msg_len) : DccPkt(msg, msg_len)
    {
    }
    uint8_t get_funcs() const;
    friend DccPkt create(const uint8_t *msg, int msg_len);
};

// 2.3.5 - Function Group Two (S-bit=0, F9-F12)
class DccPktFunc9 : public DccPkt
{
public:

    DccPktFunc9(int adrs = 3);
    virtual int set_address(int adrs) override;
    bool get_f(int num) const;
    void set_f(int num, bool on);
    static bool is_type(const uint8_t *msg, int msg_len);

private:

    static constexpr int f_min = 9;
    static constexpr int f_max = 12;
    void refresh(int adrs, uint8_t funcs = 0);
    DccPktFunc9(const uint8_t *msg, int msg_len) : DccPkt(msg, msg_len)
    {
    }
    uint8_t get_funcs() const;
    friend DccPkt create(const uint8_t *msg, int msg_len);
};

// F13 and higher are set by similar instructions

template <uint8_t i_byte, int f_min>
class DccPktFuncHi : public DccPkt
{
public:

    DccPktFuncHi(int adrs = 3)
    {
        assert(address_min <= adrs && adrs <= address_max);
        refresh(adrs);
    }

    virtual int set_address(int adrs) override
    {
        assert(address_min <= adrs && adrs <= address_max);
        refresh(adrs, get_funcs());
        return get_address_size();
    }

    bool get_f(int num) const
    {
        assert(f_min <= num && num <= (f_min + 7));
        int idx = get_address_size() + 1;   // skip address and inst byte
        uint8_t f_bit = 1 << (num - f_min); // bit for function
        return (_msg[idx] & f_bit) != 0;
    }

    void set_f(int num, bool on)
    {
        assert(f_min <= num && num <= (f_min + 7));

        int idx = get_address_size() + 1;   // skip address and inst byte
        uint8_t f_bit = 1 << (num - f_min); // bit for function
        if (on) {
            _msg[idx] |= f_bit;
        } else {
            _msg[idx] &= ~f_bit;
        }
        set_xor();
    }

    static bool is_type(const uint8_t *msg, int msg_len)
    {
        if (msg_len < 1) {
            return false;
        }
        uint8_t b0 = msg[0];
        if (1 <= b0 && b0 <= 127) {
            return msg_len == 4 && msg[1] == i_byte && DccPkt::check_xor(msg, msg_len);
        } else if (192 <= b0 && b0 <= 231) {
            return msg_len == 5 && msg[2] == i_byte && DccPkt::check_xor(msg, msg_len);
        } else {
            return false;
        }
    }

    static constexpr uint8_t inst_byte = i_byte;

private:

    void refresh(int adrs, uint8_t funcs = 0)
    {
        assert(address_min <= adrs && adrs <= address_max);
        int idx = DccPkt::set_address(adrs); // 1 or 2 bytes
        _msg[idx++] = i_byte;
        _msg[idx++] = funcs; // f_min+7:f_min+6:...:f_min+1:f_min
        _msg_len = idx + 1;  // 4 or 5
        set_xor();
    }

    DccPktFuncHi(const uint8_t *msg, int msg_len) : DccPkt(msg, msg_len)
    {
    }

    uint8_t get_funcs() const
    {
        int idx = get_address_size() + 1; // skip address and inst byte
        return _msg[idx];                 // all 8 bits
    }

    friend DccPkt create(const uint8_t *msg, int msg_len);
};

// 2.3.6.5 - F13-F20 Function Control
typedef DccPktFuncHi<0xde, 13> DccPktFunc13;

#if (DCC_FUNC_MAX >= 21)
// 2.3.6.6 - F21-F28 Function Control
typedef DccPktFuncHi<0xdf, 21> DccPktFunc21;
#endif

#if (DCC_FUNC_MAX >= 29)
// 2.3.6.7 - F29-F36 Function Control
typedef DccPktFuncHi<0xd8, 29> DccPktFunc29;
#endif

#if (DCC_FUNC_MAX >= 37)
// 2.3.6.8 - F37-F44 Function Control
typedef DccPktFuncHi<0xd9, 37> DccPktFunc37;
#endif

#if (DCC_FUNC_MAX >= 45)
// 2.3.6.9 - F45-F52 Function Control
typedef DccPktFuncHi<0xda, 45> DccPktFunc45;
#endif

#if (DCC_FUNC_MAX >= 53)
// 2.3.6.10 - F53-F60 Function Control
typedef DccPktFuncHi<0xdb, 53> DccPktFunc53;
#endif

#if (DCC_FUNC_MAX >= 61)
// 2.3.6.11 - F61-F68 Function Control
typedef DccPktFuncHi<0xdc, 61> DccPktFunc61;
#endif

// 2.3.7.3 - Configuration Variable Access - Long Form (read/verify byte)
class DccPktReadCv : public DccPkt
{
public:

    DccPktReadCv(int adrs = 3, int cv_num = 1);
    virtual int set_address(int adrs) override;
    void set_cv(int cv_num); // set cv num in message
    virtual PktType get_type() const override
    {
        return OpsRead1Cv;
    }

private:

    void refresh(int adrs, int cv_num);
    int get_cv_num() const; // get from message
};

// There is no ops "read bit" command. It does not add any functionality, and
// the returned value would just be the whole byte anyway.

// 2.3.7.3 - Configuration Variable Access - Long Form (write byte)
class DccPktWriteCv : public DccPkt
{
public:

    DccPktWriteCv(int adrs = 3, int cv_num = 1, uint8_t cv_val = 0);
    virtual int set_address(int adrs) override;
    void set_cv(int cv_num, uint8_t cv_val); // set in message
    virtual PktType get_type() const override
    {
        return OpsWriteCv;
    }

private:

    void refresh(int adrs, int cv_num, uint8_t cv_val);
    int get_cv_num() const;     // get from message
    uint8_t get_cv_val() const; // get from message
};

// 2.3.7.3 - Configuration Variable Access - Long Form (bit manipulation)
class DccPktWriteBit : public DccPkt
{
public:

    DccPktWriteBit(int adrs = address_default, int cv_num = 1, int bit_num = 0, int bit_val = 0);
    virtual int set_address(int adrs) override;
    void set_cv_bit(int cv_num, int bit_num, int bit_val);
    virtual PktType get_type() const override
    {
        return OpsWriteBit;
    }

private:

    void refresh(int adrs, int cv_num, int bit_num, int bit_val);
    int get_cv_num() const;
    int get_bit_num() const;
    int get_bit_val() const;
};

// Std 9.2.3, Section E, Service Mode Instruction Packets for Direct Mode
class DccPktSvcWriteCv : public DccPkt
{
public:

    DccPktSvcWriteCv(int cv_num = 1, uint8_t cv_val = 0);
    void set_cv(int cv_num, uint8_t cv_val);
};

// Std 9.2.3, Section E, Service Mode Instruction Packets for Direct Mode
class DccPktSvcWriteBit : public DccPkt
{
public:

    DccPktSvcWriteBit(int cv_num = 1, int bit_num = 0, int bit_val = 0);
    void set_cv_bit(int cv_num, int bit_num, int bit_val);
};

// Std 9.2.3, Section E, Service Mode Instruction Packets for Direct Mode
class DccPktSvcVerifyCv : public DccPkt
{
public:

    DccPktSvcVerifyCv(int cv_num = 1, uint8_t cv_val = 0);
    void set_cv_num(int cv_num);
    void set_cv_val(uint8_t cv_val);
};

// Std 9.2.3, Section E, Service Mode Instruction Packets for Direct Mode
class DccPktSvcVerifyBit : public DccPkt
{
public:

    DccPktSvcVerifyBit(int cv_num = 1, int bit_num = 0, int bit_val = 0);
    void set_cv_num(int cv_num);
    void set_bit(int bit_num, int bit_val);
};

DccPkt create(const uint8_t *msg, int msg_len);
