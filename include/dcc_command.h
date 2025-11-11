#pragma once

#include <cstdint>
#include <list>

#include "dcc_bitstream.h"
#include "dcc_pkt2.h"
#include "hardware/uart.h"

#undef INCLUDE_ACK_DBG

class DccAdc;
class DccThrottle;

class DccCommand
{
public:

    DccCommand(int sig_gpio, int pwr_gpio, int slp_gpio, DccAdc &adc,
               uart_inst_t *const rc_uart = nullptr, int rc_gpio = -1);
    ~DccCommand();

    void mode_off();
    void mode_ops();

    void write_cv(int cv_num, uint8_t cv_val);
    void write_bit(int cv_num, int bit_num, int bit_val);
    void read_cv(int cv_num);
    void read_bit(int cv_num, int bit_num);

    // Returns true if service mode operation is done, and result is set
    // true (success) or false (failed). For read operations, use the one
    // with val to get the result.
    bool svc_done(bool &result);
    bool svc_done(bool &result, uint8_t &val);

    enum class Mode {
        OFF,
        OPS,
        SVC,
    };

    Mode mode() const
    {
        return _mode;
    }

    DccAdc &adc() const
    {
        return _adc;
    }

    // called by DccBitstream to get a packet to send
    void get_packet(DccPkt2 &pkt);

    void loop();

    DccThrottle *find_throttle(int address);
    DccThrottle *create_throttle(int address = DccPkt::address_default);
    DccThrottle *delete_throttle(DccThrottle *throttle);
    DccThrottle *delete_throttle(int address);
    void restart_throttles();

    void show();

private:

    DccBitstream _bitstream;

    DccAdc &_adc;

    Mode _mode;

    enum class ModeSvc {
        NONE,
        WRITE_CV,
        WRITE_BIT,
        READ_CV,
        READ_BIT,
    };

    ModeSvc _mode_svc;

    std::list<DccThrottle *> _throttles;
    std::list<DccThrottle *>::iterator _next_throttle;

    void get_packet_ops(DccPkt2 &pkt);

    // used by write_cv(), write_bit(), read_cv(), and read_bit()
    void svc_start();

    // for CV operations
    enum CvOp {
        IN_PROGRESS,
        SUCCESS,
        ERROR,
    };

    CvOp _svc_status, _svc_status_next;
    uint16_t _ack_ma;
    static const uint16_t ack_inc_ma = 60;

    int _reset1_cnt;
    int _command_cnt;
    int _reset2_cnt;

    DccPktReset _pkt_reset;

    // for service mode write byte or bit
    DccPktSvcWriteCv _pkt_svc_write_cv;
    DccPktSvcWriteBit _pkt_svc_write_bit;
    void get_packet_svc_write(DccPkt2 &pkt);

    // for service mode verify byte or bit
    DccPktSvcVerifyCv _pkt_svc_verify_cv;
    DccPktSvcVerifyBit _pkt_svc_verify_bit;
    int _verify_bit;
    int _verify_bit_val; // 0 or 1
    uint8_t _cv_val;
    void get_packet_svc_read_cv(DccPkt2 &pkt);
    void get_packet_svc_read_bit(DccPkt2 &pkt);

    void assert_svc_idle();
};
