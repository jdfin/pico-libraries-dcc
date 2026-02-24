// Stub DccAdc for native tests
// Provides controllable return values for testing DccCommand

#include "dcc_adc.h"

// Controllable stub state
static uint16_t _stub_short_avg_ma = 0;
static uint16_t _stub_long_avg_ma = 100;
static int _stub_loop_result = 0;
// Control functions for tests
void stub_adc_set_short_avg_ma(uint16_t val) { _stub_short_avg_ma = val; }
void stub_adc_set_long_avg_ma(uint16_t val) { _stub_long_avg_ma = val; }
void stub_adc_set_loop_result(int val) { _stub_loop_result = val; }

// DccAdc implementation stubs
DccAdc::DccAdc(int gpio) :
    _gpio(gpio),
    _avg_idx(0),
    _err_cnt(0),
    _log_max(0),
    _log_idx(0),
    _log(nullptr),
    _dbg_loop_gpio(-1)
{
    for (int i = 0; i < avg_max; i++)
        _avg[i] = 0;
}

DccAdc::~DccAdc() {}

void DccAdc::start() {}
void DccAdc::stop() {}

int DccAdc::loop() { return _stub_loop_result; }

uint16_t DccAdc::short_avg_ma() const { return _stub_short_avg_ma; }
uint16_t DccAdc::long_avg_ma() const { return _stub_long_avg_ma; }

uint16_t DccAdc::avg_raw(int cnt) const { (void)cnt; return 0; }

void DccAdc::log_init(int samples) { (void)samples; }
void DccAdc::log_reset() {}
void DccAdc::log_show() const {}
