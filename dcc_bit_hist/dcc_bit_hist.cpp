
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
// pico
#include "hardware/clocks.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
// pio_edges
#include "pio_edges.h"
// dcc
#include "dcc_spec.h"
// misc
#include "pretty_io.h"
#include "sys_led.h"
//
#include "dcc_gpio_cfg.h"

// Record histogram of DCC intervals

// Units in variable names:
//  _tk ticks (received from pio)
//  _ns nanoseconds
//  _us microseconds
//  _ms milliseconds
//  _hz Hertz
//  _ct counts (unitless)

static uint32_t pio_tick_hz = 0;

// Histogram bin index is pio ticks (_tk)
static const int hist_max_us = 500; // last bin, usec
static int hist_max_tk = 0;         // last bin, ticks
static uint16_t *hist_hi_ct = nullptr;
static uint16_t *hist_lo_ct = nullptr;
static uint32_t hist_ct = 0;

//static uint32_t hi

// How many intervals to histogram
// The bitstream runs roughly 10,000 bits/sec.
// Set to UINT32_MAX to run until any bin fills up.
// 100,000 half-bits takes about 8 seconds.
// Until-full takes a minute or two
static uint32_t hist_end_ct = 100000; //UINT32_MAX;

#if 0
// Rising edges are adjusted for the slow rise time
static const uint32_t adj_ns = 64; //440;
static uint32_t adj_tk = 0;
#endif

static inline int us_to_tk(int us)
{
    return (int64_t(us) * pio_tick_hz + 500'000) / 1'000'000;
}


static inline int ns_to_tk(int ns)
{
    return (int64_t(ns) * pio_tick_hz + 500'000'000LL) / 1'000'000'000LL;
}


static void init()
{
    stdio_init_all();

    SysLed::init();

    // enable stdio usb in CMakeLists.txt
#if LIB_PICO_STDIO_USB
    SysLed::pattern(50, 950);

    while (!stdio_usb_connected()) {
        tight_loop_contents();
        SysLed::loop();
    }

    // With no delay here, we lose the first few lines of output (to usb serial).
    // Delaying 1 msec has been observed to work with a debug build.
    sleep_ms(10);
#endif

    printf("\n");
    printf("DCC Bit Histogram\n");
    printf("\n");

    SysLed::pattern(50, 1950);

    Edges::init(dcc_sig_gpio);

    uint32_t sys_clk_hz = clock_get_hz(clk_sys);

    printf("sys_clk_hz = %s\n", pretty_hz(sys_clk_hz));

    pio_tick_hz = Edges::get_tick_hz();
    assert(pio_tick_hz > 0);
    printf("pio_tick_hz = %s (gpio sampling rate)\n", pretty_hz(pio_tick_hz));

    uint32_t pio_tick_ns = Edges::tick_to_nsec(1);
    printf("pio_tick_ns = %lu nsec/tick", pio_tick_ns);
    if ((pio_tick_hz * pio_tick_ns) != 1'000'000'000)
        printf(" (inexact)");
    printf("\n");

#if 0
    adj_tk = ns_to_tk(adj_ns);
    printf("adj_ns = %lu, adj_tk = %lu (subtracted from rising edges)\n", adj_ns, adj_tk);
#endif

    // allocate histograms so the last bin is hist_max_us
    hist_max_tk = uint64_t(hist_max_us) * uint64_t(pio_tick_hz) / 1'000'000;
    printf("hist_max_us = %d, hist_max_tk = %d (bins)\n", hist_max_us,
           hist_max_tk);

    hist_hi_ct = new uint16_t[hist_max_tk];
    assert(hist_hi_ct != nullptr);
    memset(hist_hi_ct, 0, hist_max_tk * sizeof(uint16_t));

    hist_lo_ct = new uint16_t[hist_max_tk];
    assert(hist_lo_ct != nullptr);
    memset(hist_lo_ct, 0, hist_max_tk * sizeof(uint16_t));

    if (hist_end_ct == UINT32_MAX)
        printf("collecting intervals until full ...\n");
    else
        printf("collecting %lu intervals ...\n", hist_end_ct);

} // static void init()


static void print_histogram()
{
    // DCC spec numbers converted to ticks
    const int min_1_tk = us_to_tk(DccSpec::tr1_min_us);
    const int max_1_tk = us_to_tk(DccSpec::tr1_max_us);
    const int min_0_tk = us_to_tk(DccSpec::tr0_min_us);
    const int max_0_tk = us_to_tk(DccSpec::tr0_max_us);

    // calculate averages of high and low intervals for each histogram

    // '1' half-bits
    uint32_t hi_1_sum_tk = 0; // hi intervals
    uint32_t hi_1_ct = 0;
    uint32_t lo_1_sum_tk = 0; // lo intervals
    uint32_t lo_1_ct = 0;

    // '0' half-bits
    uint32_t hi_0_sum_tk = 0; // hi intervals
    uint32_t hi_0_ct = 0;
    uint32_t lo_0_sum_tk = 0; // lo intervals
    uint32_t lo_0_ct = 0;

    char bit_type = '?'; // '0', '1', or ' ' (neither)

    printf("\n");
    printf("ticks    nsec        hi    lo\n");
    //      ddddd ddddddd  c  uuuuu uuuuu

    for (int tk = 0; tk < hist_max_tk; tk++) {

        if (hist_hi_ct[tk] == 0 && hist_lo_ct[tk] == 0)
            continue;

        if (min_0_tk <= tk && tk <= max_0_tk) {
            if (bit_type != '0')
                printf("-----------------------------\n");
            bit_type = '0';
            lo_0_sum_tk += (tk * hist_lo_ct[tk]);
            lo_0_ct += hist_lo_ct[tk];
            hi_0_sum_tk += (tk * hist_hi_ct[tk]);
            hi_0_ct += hist_hi_ct[tk];
        } else if (min_1_tk <= tk && tk <= max_1_tk) {
            if (bit_type != '1')
                printf("-----------------------------\n");
            bit_type = '1';
            lo_1_sum_tk += (tk * hist_lo_ct[tk]);
            lo_1_ct += hist_lo_ct[tk];
            hi_1_sum_tk += (tk * hist_hi_ct[tk]);
            hi_1_ct += hist_hi_ct[tk];
        } else {
            if (bit_type != ' ')
                printf("-----------------------------\n");
            bit_type = ' ';
        }

        printf("%5d %7llu  %c  %5u %5u\n", tk, Edges::tick_to_nsec(tk),
               bit_type, hist_hi_ct[tk], hist_lo_ct[tk]);

    } // for (uint32_t tk...)

    printf("-----------------------------\n");
    printf("\n");

    printf("averages:\n");

    int32_t avg_lo_1_tk = (lo_1_sum_tk + lo_1_ct / 2) / lo_1_ct;
    int32_t avg_hi_1_tk = (hi_1_sum_tk + hi_1_ct / 2) / hi_1_ct;
    int32_t avg_lo_0_tk = (lo_0_sum_tk + lo_0_ct / 2) / lo_0_ct;
    int32_t avg_hi_0_tk = (hi_0_sum_tk + hi_0_ct / 2) / hi_0_ct;

    int64_t avg_lo_1_ns = Edges::tick_to_nsec(avg_lo_1_tk);
    int64_t avg_hi_1_ns = Edges::tick_to_nsec(avg_hi_1_tk);
    int64_t avg_lo_0_ns = Edges::tick_to_nsec(avg_lo_0_tk);
    int64_t avg_hi_0_ns = Edges::tick_to_nsec(avg_hi_0_tk);

    printf("  half-one:  lo %ld_tk = %6lld_ns, hi %ld_tk = %6lld_ns\n",
           avg_lo_1_tk, avg_lo_1_ns, avg_hi_1_tk, avg_hi_1_ns);
    printf("  half-zero: lo %ld_tk = %6lld_ns; hi %ld_tk = %6lld_ns\n",
           avg_lo_0_tk, avg_lo_0_ns, avg_hi_0_tk, avg_hi_0_ns);
    printf("\n");

    // if low intervals are longer than high intervals, then rising edges are delayed
    // averages must be signed integers here

    float rise_delay =
        (avg_lo_0_tk - avg_hi_0_tk + avg_lo_1_tk - avg_hi_1_tk) / 4.0;
    if (rise_delay >= 0.0)
        printf("rising edges are delayed %0.2f ticks (%0.0f nsec)\n",
               rise_delay, rise_delay * 1000000000.0 / Edges::get_tick_hz());
    else
        printf("falling edges are delayed %0.2f ticks (%0.0f nsec)\n",
               -rise_delay, -rise_delay * 1000000000.0 / Edges::get_tick_hz());
    printf("\n");

    // print reminder of what valid bits are

    printf("dcc spec:\n");
    printf("  half-one:  %d_us - %d_us - %d_us\n", DccSpec::tr1_min_us,
           DccSpec::tr1_nom_us, DccSpec::tr1_max_us);
    printf("  half-zero: %d_us - %d_us - %d_us\n", DccSpec::tr0_min_us,
           DccSpec::tr0_nom_us, DccSpec::tr0_max_us);

} // static void print_histogram()


static void loop()
{
    int rise;
    uint64_t edge_tk;
    if (!Edges::get_tick(rise, edge_tk))
        return;

    // When decoding DCC, we don't care which edge it is, but when looking
    // for asymmetry between rising and falling edges, it's nice to know.
    // Asymmetry happens when one edge (falling) is nice & sharp, and the
    // other (rising) is slow. If you look at a trace and see where the
    // transition start, the fast edge gets its timestamp right away but
    // the slow edge gets its timestamp a tiny bit later. The histogram
    // printout shows this clearly.

#if 0
    if (rise == 1)
        edge_tk -= ((adj_ns * uint64_t(pio_tick_hz) + 500'000'000ULL) / 1'000'000'000ULL);
#endif

    static uint64_t edge_prv_tk = UINT64_MAX;

    if (edge_prv_tk != UINT64_MAX) {

        uint32_t edge_int_tk = edge_tk - edge_prv_tk;

        if (edge_int_tk >= uint32_t(hist_max_tk))
            edge_int_tk = hist_max_tk - 1; // cap at last bin

        // hist_lo_ct is low intervals (fall to rise) and
        // hist_hi_ct is high intervals (rise to fall)

        uint32_t bin_ct;
        if (rise == 1)
            bin_ct = ++(hist_lo_ct[edge_int_tk]);
        else
            bin_ct = ++(hist_hi_ct[edge_int_tk]);

        // Continue until hist_ct >= hist_end_ct, or until any bin is full,
        // whichever comes first
        if (bin_ct == UINT16_MAX)
            hist_end_ct = hist_ct; // bit is full, force done
        else
            hist_ct++; // bit is not full, just keep counting

        if (hist_ct >= hist_end_ct) {
            print_histogram();
            SysLed::on();
        }

    } // if (edge_prv_tk != UINT32_MAX)

    edge_prv_tk = edge_tk;

} // static void loop()


int main()
{
    init();

    uint64_t start_us = time_us_64();

    do {
        tight_loop_contents();
        SysLed::loop();
        loop();
    } while (hist_ct < hist_end_ct);

    printf("\n");
    printf("done in %0.1f sec\n", (time_us_64() - start_us) / 1000000.0);

    sleep_ms(100); // let prints finish

    return 0;

} // int main()
