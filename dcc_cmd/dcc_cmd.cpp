
#include <strings.h>
//
#include <cassert>
#include <cstdint>
#include <cstdio>
// pico
#include "hardware/sync.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "pico/time.h"
// misc
#include "argv.h"
#include "str_ops.h"
#include "sys_led.h"
// dcc
#include "dcc_api.h"
#include "dcc_gpio_cfg.h"
#include "dcc_pkt.h"

// A command is a sequence of tokens, ending with newline.
//
// The "Argv" object takes a character at a time and builds a complete command
// (an array of tokens), which can then be examined for commands and parameters.

static Argv argv;

// These functions look at commands and see if there are any valid commands
// to process.

static bool cmd_try();
static bool track_try();
static bool cv_try();
static bool addr_try();
static bool loco_try();
static bool debug_try();

static void cmd_help();
static void track_help();
static void cv_help();
static void addr_help();
static void loco_help();
static void debug_help();
static void param_help();
static void print_help(const char *help_short, const char *help_long);


// function to get notification messages
static bool dcc_cmd_notify(intptr_t, const char *msg)
{
    printf("notify: \"%s\"\n", msg);
    return true;
}


static inline uint32_t usec_to_msec(uint64_t us)
{
    return (uint32_t)((us + 500) / 1000);
}


int main()
{
    stdio_init_all();

    SysLed::init();
    SysLed::pattern(50, 950);

    while (!stdio_usb_connected()) {
        tight_loop_contents();
        SysLed::loop();
    }

    sleep_ms(10);

    SysLed::off();

    printf("\n");
    printf("dcc_cmd\n");
    printf("\n");

    argv.verbosity(1);

    DccApi::init(dcc_sig_gpio, dcc_pwr_gpio, dcc_adc_gpio, dcc_rcom_gpio,
                 dcc_rcom_uart);

    DccApi::notify(dcc_cmd_notify);

    printf("\n");
    cmd_help();
    printf("\n");

    while (true) {

        DccApi::loop();

        // Get console input if available.
        // This might result in a new command.
        int c = stdio_getchar_timeout_us(0);
        if (0 <= c && c <= 255) {
            if (argv.add_char(char(c))) {
                // newline received, try to process command
                if (!cmd_try()) {
                    // command not recognized
                    printf("ERROR");
                    printf(": invalid command: ");
                    argv.print();
                    printf("\n");
                    cmd_help();
                    printf("\n");
                }
                argv.reset();
            }
        }
    } // while (true)

    return 0;
}


static bool cmd_try()
{
    assert(argv.argc() > 0);

    if (strcasecmp(argv[0], "T") == 0)
        return track_try();
    else if (strcasecmp(argv[0], "C") == 0)
        return cv_try();
    else if (strcasecmp(argv[0], "A") == 0)
        return addr_try();
    else if (strcasecmp(argv[0], "L") == 0)
        return loco_try();
    else if (strcasecmp(argv[0], "D") == 0)
        return debug_try();
    else
        return false;
}


static void cmd_help()
{
    printf("Commands:\n");
    track_help();
    cv_help();
    addr_help();
    loco_help();
    debug_help();
    printf("\n");
    param_help();
}


static bool track_try()
{
    assert(argv.argc() >= 1);
    assert(strcasecmp(argv[0], "T") == 0);

    if (argv.argc() < 2)
        return false;

    if (strcasecmp(argv[1], "G") == 0) {
        if (argv.argc() != 2) {
            return false;
        } else {
            printf("track_get ... ");
            bool on;
            DccApi::Status s = DccApi::track_get(on);
            if (s == DccApi::Status::Ok)
                printf("%s ... ", on ? "on" : "off");
            printf("[%s]\n", DccApi::status(s));
            return true;
        }
    } else if (strcasecmp(argv[1], "S") == 0) {
        if (argv.argc() != 3) {
            return false;
        } else if (strcmp(argv[2], "0") == 0) {
            printf("track_set ... ");
            DccApi::Status s = DccApi::track_set(false);
            printf("[%s]\n", DccApi::status(s));
            return true;
        } else if (strcmp(argv[2], "1") == 0) {
            printf("track_set ... ");
            DccApi::Status s = DccApi::track_set(true);
            printf("[%s]\n", DccApi::status(s));
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}


static void track_help()
{
    print_help("T G", "track_get");
    print_help("T S 0|1", "track_set");
}


static bool cv_try()
{
    assert(argv.argc() >= 1);
    assert(strcasecmp(argv[0], "C") == 0);

    // C <c> G              - cv_val_get
    // C <c> S <v>          - cv_val_set
    // C <c> B <b> G        - cv_bit_get
    // C <c> B <b> S 0|1    - cv_bit_set

    if (argv.argc() < 2)
        return false;

    int cv_num;
    if (!str_to_int(argv[1], &cv_num))
        return false; // error parsing cv number

    // server checks that cv_num is in range

    if (argv.argc() < 3)
        return false;

    const char *cmd = argv[2];

    if (strcasecmp(cmd, "G") == 0) {
        // read cv
        if (argv.argc() != 3) {
            return false; // junk after 'G'
        } else {
            printf("cv_val_get ... ");
            int cv_val;
            DccApi::Status s = DccApi::cv_val_get(cv_num, cv_val);
            if (s == DccApi::Status::Ok)
                printf("%u = 0x%02x ... ", unsigned(cv_val), unsigned(cv_val));
            printf("[%s]\n", DccApi::status(s));
            return true;
        }
    } else if (strcasecmp(cmd, "S") == 0) {
        // write cv
        if (argv.argc() != 4) {
            return false; // should be "C <c> S <v>"
        } else {
            int cv_val;
            if (!str_to_int(argv[3], &cv_val))
                return false; // error parsing cv value
            // server checks that cv_val is in range
            printf("cv_val_set ... ");
            DccApi::Status s = DccApi::cv_val_set(cv_num, cv_val);
            printf("[%s]\n", DccApi::status(s));
            return true;
        }
    } else if (strcasecmp(cmd, "B") == 0) {

        // C <c> B <b> G        - read cv number <c> bit <b>
        // C <c> B <b> S 0|1    - write cv number <c> bit <b> with 0/1

        if (argv.argc() < 5) // should be 5 or 6
            return false;

        int bit_num;
        if (!str_to_int(argv[3], &bit_num))
            return false; // error parsing bit number

        const char *subcmd = argv[4];

        if (strcasecmp(subcmd, "G") == 0) {
            // read cv bit
            if (argv.argc() != 5) {
                return false; // junk after 'G'
            } else {
                printf("cv_bit_get ... ");
                int bit_val;
                DccApi::Status s = DccApi::cv_bit_get(cv_num, bit_num, bit_val);
                if (s == DccApi::Status::Ok)
                    printf("%d ... ", bit_val);
                printf("[%s]\n", DccApi::status(s));
                return true;
            }
        } else if (strcasecmp(subcmd, "S") == 0) {
            // write cv bit
            if (argv.argc() != 6) {
                return false; // should be "C <c> B <b> S <v>"
            } else {
                int bit_val;
                if (!str_to_int(argv[5], &bit_val))
                    return false; // error parsing bit value
                printf("cv_bit_set ... ");
                DccApi::Status s = DccApi::cv_bit_set(cv_num, bit_num, bit_val);
                printf("[%s]\n", DccApi::status(s));
                return true;
            }
        } else {
            return false; // unknown subcmd
        }

    } else {
        return false; // "C <c> <cmd>", unknown <cmd>
    }

} // cv_try


static void cv_help()
{
    print_help("C <c> G", "cv_val_get");
    print_help("C <c> S <v>", "cv_val_set");
    print_help("C <c> B <b> G", "cv_bit_get");
    print_help("C <c> B <b> S 0|1", "cv_bit_set");
}


static bool addr_try()
{
    assert(argv.argc() >= 1);
    assert(strcasecmp(argv[0], "A") == 0);

    // A G              - addr_get
    // A S <a>          - addr_set

    if (argv.argc() < 2)
        return false;

    const char *cmd = argv[1];

    if (strcasecmp(cmd, "G") == 0) {
        // read address
        if (argv.argc() != 2) {
            return false; // junk after 'G'
        } else {
            printf("addr_get ... ");
            int addr;
            DccApi::Status s = DccApi::addr_get(addr);
            if (s == DccApi::Status::Ok)
                printf("%d ... ", addr);
            printf("[%s]\n", DccApi::status(s));
            return true;
        }
    } else if (strcasecmp(cmd, "S") == 0) {
        // write address
        if (argv.argc() != 3) {
            return false; // should be "A S <a>"
        } else {
            int addr;
            if (!str_to_int(argv[2], &addr))
                return false; // error parsing address
            // server checks that address is in range
            printf("addr_set ... ");
            DccApi::Status s = DccApi::addr_set(addr);
            printf("[%s]\n", DccApi::status(s));
            return true;
        }
    } else {
        return false; // "A <cmd>", unknown <cmd>
    }

} // addr_try


static void addr_help()
{
    print_help("A G", "addr_get");
    print_help("A S <v>", "addr_set");
}


static bool loco_try()
{
    assert(argv.argc() >= 1);
    assert(strcasecmp(argv[0], "L") == 0);

    // L <addr> N                                  - loco_create
    // L <addr> D                                  - loco_delete
    // L <addr> F <func> G                         - loco_func_get
    // L <addr> F <func> S <0|1>                   - loco_func_set
    // L <addr> S G                                - loco_speed_get
    // L <addr> S S <speed>                        - loco_speed_set
    // L <addr> C <cv_num> G                       - loco_cv_val_get
    // L <addr> C <cv_num> S <cv_val>              - loco_cv_val_set
    // L <addr> C <cv_num> B <bit_num> S <bit_val> - loco_cv_bit_set

    if (argv.argc() < 2)
        return false;

    // argv[1] is always address
    int addr;
    if (str_to_int(argv[1], &addr) != 1)
        return false; // error parsing address

    // server checks that address is in range

    if (argv.argc() < 3)
        return false;

    const char *cmd = argv[2];

    if (strcasecmp(cmd, "N") == 0) {
        if (argv.argc() != 3)
            return false; // junk after 'N'
        printf("loco_create ... ");
        DccApi::Status s = DccApi::loco_create(addr);
        printf("[%s]\n", DccApi::status(s));
        return true;
    } else if (strcasecmp(cmd, "D") == 0) {
        if (argv.argc() != 3)
            return false; // junk after 'D'
        printf("loco_delete ... ");
        DccApi::Status s = DccApi::loco_delete(addr);
        printf("[%s]\n", DccApi::status(s));
        return true;
    } else if (strcasecmp(cmd, "F") == 0) {
        // argv[3] is function number
        if (argv.argc() < 4)
            return false;
        int func;
        if (str_to_int(argv[3], &func) != 1)
            return false; // error parsing function number
        // argv[4] is subcommand
        if (argv.argc() < 5)
            return false;
        const char *subcmd = argv[4];
        if (strcasecmp(subcmd, "G") == 0) {
            if (argv.argc() != 5)
                return false; // junk after 'G'
            printf("loco_func_get ... ");
            bool on;
            DccApi::Status s = DccApi::loco_func_get(addr, func, on);
            if (s == DccApi::Status::Ok)
                printf("%s ... ", on ? "on" : "off");
            printf("[%s]\n", DccApi::status(s));
            return true;
        } else if (strcasecmp(subcmd, "S") == 0) {
            if (argv.argc() != 6)
                return false; // should be "L <addr> F <func> S <setting>"
            int setting;
            if (str_to_int(argv[5], &setting) != 1)
                return false; // error parsing setting
            if (setting != 0 && setting != 1)
                return false; // setting should be 0 or 1
            printf("loco_func_set ... ");
            DccApi::Status s = DccApi::loco_func_set(addr, func, setting != 0);
            printf("[%s]\n", DccApi::status(s));
            return true;
        } else {
            return false; // "L <addr> F <func> <cmd>", unknown <cmd>
        }
    } else if (strcasecmp(cmd, "S") == 0) {
        if (argv.argc() < 4)
            return false; // need at least "L <addr> S G" or "L <addr> S S <speed>"
        if (strcasecmp(argv[3], "G") == 0) {
            if (argv.argc() != 4)
                return false; // junk after 'G'
            printf("loco_speed_get ... ");
            int speed;
            DccApi::Status s = DccApi::loco_speed_get(addr, speed);
            if (s == DccApi::Status::Ok)
                printf("%d ... ", speed);
            printf("[%s]\n", DccApi::status(s));
            return true;
        } else if (strcasecmp(argv[3], "S") == 0) {
            if (argv.argc() != 5)
                return false; // should be "L <addr> S S <speed>"
            int speed;
            if (str_to_int(argv[4], &speed) != 1)
                return false; // error parsing speed
            printf("loco_speed_set ... ");
            DccApi::Status s = DccApi::loco_speed_set(addr, speed);
            printf("[%s]\n", DccApi::status(s));
            return true;
        } else {
            return false; // "L <addr> S <cmd>", unknown <cmd>
        }
    } else if (strcasecmp(cmd, "C") == 0) {
        // argv[3] is cv number
        if (argv.argc() < 4)
            return false;
        int cv_num;
        if (str_to_int(argv[3], &cv_num) != 1)
            return false; // error parsing cv number
        // argv[4] is subcommand
        if (argv.argc() < 5)
            return false;
        const char *subcmd = argv[4];
        if (strcasecmp(subcmd, "G") == 0) {
            if (argv.argc() != 5)
                return false; // junk after 'G'
            printf("loco_cv_val_get ... ");
            int cv_val;
            DccApi::Status s = DccApi::loco_cv_val_get(addr, cv_num, cv_val);
            if (s == DccApi::Status::Ok)
                printf("%u = 0x%02x ... ", unsigned(cv_val), unsigned(cv_val));
            printf("[%s]\n", DccApi::status(s));
            return true;
        } else if (strcasecmp(subcmd, "S") == 0) {
            if (argv.argc() != 6)
                return false; // should be "L <addr> C <cv_num> S <cv_val>"
            int cv_val;
            if (str_to_int(argv[5], &cv_val) != 1)
                return false; // error parsing cv_val
            printf("loco_cv_val_set ... ");
            DccApi::Status s = DccApi::loco_cv_val_set(addr, cv_num, cv_val);
            printf("[%s]\n", DccApi::status(s));
            return true;
        } else if (strcasecmp(subcmd, "B") == 0) {
            if (argv.argc() != 8)
                return false; // should be "L <addr> C <cv_num> B <bit_num> S <bit_val>"
            int bit_num;
            if (str_to_int(argv[5], &bit_num) != 1)
                return false; // error parsing bit_num
            if (strcasecmp(argv[6], "S") != 0)
                return false; // should be "S"
            int bit_val;
            if (str_to_int(argv[7], &bit_val) != 1)
                return false; // error parsing bit_val
            printf("loco_cv_bit_set ... ");
            DccApi::Status s =
                DccApi::loco_cv_bit_set(addr, cv_num, bit_num, bit_val);
            printf("[%s]\n", DccApi::status(s));
            return true;
        } else {
            return false; // "L <addr> C <cv_num> <cmd>", unknown <cmd>
        }
    } else {
        // "L <addr> <cmd>", unknown <cmd>
        return false;
    }

} // loco_try


static void loco_help()
{
    print_help("L <a> N", "loco_create");
    print_help("L <a> D", "loco_delete");
    print_help("L <a> F <f> G", "loco_func_get");
    print_help("L <a> F <f> S 0|1", "loco_func_set");
    print_help("L <a> S G", "loco_speed_get");
    print_help("L <a> S S <s>", "loco_speed_set");
    print_help("L <a> C <n> G", "loco_cv_val_get");
    print_help("L <a> C <n> S <v>", "loco_cv_val_set");
    print_help("L <a> C <n> B <b> S <v>", "loco_cv_bit_set");
}


static bool debug_try()
{
    assert(argv.argc() >= 1);
    assert(strcasecmp(argv[0], "D") == 0);

    // D <code> G       - get debug setting
    // D <code> S <val> - set debug setting

    // argv[1] is always <code>
    int code;
    if (argv.argc() < 2 || str_to_int(argv[1], &code) != 1)
        return false; // error parsing code

    // argv[2] is always cmd (G or S)
    if (argv.argc() < 3)
        return false;
    const char *cmd = argv[2];

    if (strcasecmp(cmd, "G") == 0) {
        if (argv.argc() != 3)
            return false; // should be "D <code> G"
        printf("debug_get ... ");
        int val;
        DccApi::Status s = DccApi::debug_get(code, val);
        if (s == DccApi::Status::Ok)
            printf("%d ... ", val);
        printf("[%s]\n", DccApi::status(s));
        return true;
    } else if (strcasecmp(cmd, "S") == 0) {
        if (argv.argc() != 4)
            return false; // should be "D <code> S <val>"
        int val;
        if (str_to_int(argv[3], &val) != 1)
            return false; // error parsing value
        printf("debug_set ... ");
        DccApi::Status s = DccApi::debug_set(code, val);
        printf("[%s]\n", DccApi::status(s));
        return true;
    } else {
        // "D <cmd>", unknown <cmd>
        return false;
    }

} // debug_try


static void debug_help()
{
    print_help("D <code> G", "get debug value for code");
    print_help("D <code> S <value>", "set debug value for code");
}


static void param_help()
{
    int n;
    constexpr int tab_col = 20;

    printf("Parameters:\n");

    n = printf("%d <= a <= %d", DccPkt::address_min, DccPkt::address_max);
    while (++n < tab_col) {
        printf(" ");
    }
    printf("loco address\n");

    n = printf("%d <= s <= %d", DccPkt::speed_min, DccPkt::speed_max);
    while (++n < tab_col) {
        printf(" ");
    }
    printf("loco speed\n");

    n = printf("%d <= f <= %d", DccPkt::function_min, DccPkt::function_max);
    while (++n < tab_col) {
        printf(" ");
    }
    printf("function number\n");

    n = printf("%d <= c <= %d", DccPkt::cv_num_min, DccPkt::cv_num_max);
    while (++n < tab_col) {
        printf(" ");
    }
    printf("cv number\n");

    n = printf("%d <= v <= %d", DccPkt::cv_val_min, DccPkt::cv_val_max);
    while (++n < tab_col) {
        printf(" ");
    }
    printf("cv value\n");

    n = printf("%d <= b <= %d", 0, 7);
    while (++n < tab_col) {
        printf(" ");
    }
    printf("bit number\n");
}


static void print_help(const char *help_short, const char *help_long)
{
    printf("%-24s", help_short);
    printf(help_long);
    printf("\n");
}
