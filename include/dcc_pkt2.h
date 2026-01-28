#pragma once

#include <cassert>

#include "dcc_pkt.h"

// DccPkt2 knows:
//   packet data (currently in DccPkt)
//   overall length (in DccPkt)
//   sending throttle
//
// The objective is for DccBitstream to get one of these to send a packet, and
// if a (RailCom) response is received, to be able to notify the throttle of
// the response.
//
// A longer-term objective is to refactor DccPkt so it is not bit-twiddling so
// much, and simply encodes and decodes as packets are sent and received.

class DccThrottle;

class DccPkt2
{

public:

    DccPkt2() : _pkt(), _throttle(nullptr)
    {
    }

    DccPkt2(const DccPkt &pkt, DccThrottle *throttle = nullptr) :
        _pkt(pkt), _throttle(throttle)
    {
    }

    // Change the packet and throttle.
    // Hopefully these go away.

    void set_pkt(DccPkt pkt)
    {
        _pkt = pkt;
    }

    void set_throttle(DccThrottle *throttle)
    {
        _throttle = throttle;
    }

    void set(DccPkt pkt, DccThrottle *throttle = nullptr)
    {
        _pkt = pkt;
        _throttle = throttle;
    }

    DccThrottle *get_throttle() const
    {
        return _throttle;
    }

    // length, including address, instruction, check byte
    int len() const
    {
        return _pkt.msg_len();
    }

    // data byte
    uint8_t data(int idx)
    {
        assert(idx >= 0 && idx < _pkt.msg_len());
        return _pkt.data(idx);
    }

    char *show(char *buf, int buf_len) const
    {
        return _pkt.show(buf, buf_len);
    }

    DccPkt::PktType get_type() const
    {
        return _pkt.get_type();
    }

private:

    DccPkt _pkt;

    DccThrottle *_throttle;

}; // class DccPkt2
