#pragma once

#include <cassert>

#include "dcc_pkt.h"

// DccPkt2 knows:
//   packet data (currently in DccPkt)
//   overall length (in DccPkt)
//   sending loco
//
// The objective is for DccBitstream to get one of these to send a packet, and
// if a (RailCom) response is received, to be able to notify the loco of the
// response.
//
// A longer-term objective is to refactor DccPkt so it is not bit-twiddling so
// much, and simply encodes and decodes as packets are sent and received.

class DccLoco;

class DccPkt2
{

public:

    DccPkt2() : _pkt(), _loco(nullptr)
    {
    }

    DccPkt2(const DccPkt &pkt, DccLoco *loco = nullptr) :
        _pkt(pkt), _loco(loco)
    {
    }

    void set(DccPkt pkt, DccLoco *loco = nullptr)
    {
        _pkt = pkt;
        _loco = loco;
    }

    DccLoco *get_loco() const
    {
        return _loco;
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

    DccLoco *_loco;

}; // class DccPkt2
