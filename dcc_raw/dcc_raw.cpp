
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
#include "buf_log.h"
#include "sys_led.h"
// dcc
#include "dcc_api.h"
#include "dcc_gpio_cfg.h"
#include "dcc_pkt.h"
#include "railcom.h"

//int RailCom::dbg_junk = 22;

#if 0
// function to get notification messages
static bool dcc_cmd_notify(intptr_t, const char *msg)
{
    printf("notify: \"%s\"\n", msg);
    return true;
}
#endif


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
    printf("dcc_raw\n");
    printf("\n");

    RailCom::dbg_init();

    DccApi::init(dcc_sig_gpio, dcc_pwr_gpio, dcc_adc_gpio, dcc_rcom_gpio,
                 dcc_rcom_uart);

    //DccApi::notify(dcc_cmd_notify, 0);

    char req[req_msg_len_max];
    uint req_len = 0;

    while (true) {
        DccApi::loop();
        int c = stdio_getchar_timeout_us(0);
        if (0 <= c && c <= 255) {
            if (c == '\r' || c == '\n') {
                // cr or lf received, try to process command
                if (req_len > 0) {
                    printf("\n");
                    // replace the cr/lf with null
                    req[req_len] = '\0';
                    //printf("req: \"%s\"\n", req);
                    if (DccApi::raw_req(req) != DccApi::Status::Ok)
                        printf("req_send() failed\n");
                    req_len = 0;
                }
            } else if (req_len < (req_msg_len_max - 1)) {
                printf("%c", c); // echo
                req[req_len++] = char(c);
            }
        }

        // check for any responses
        char rsp_msg[rsp_msg_len_max];
        if (DccApi::raw_rsp(rsp_msg, sizeof(rsp_msg), 0) == DccApi::Status::Ok) {
            printf("rsp: \"%s\"\n", rsp_msg);
            //printf("%s\n", rsp_msg);
        }

        // check for any notifications
        char not_msg[not_msg_len_max];
        if (DccApi::raw_not(not_msg, sizeof(not_msg), 0) == DccApi::Status::Ok) {
            printf("not: \"%s\"\n", not_msg);
            //printf("%s\n", not_msg);
        }

        BufLog::loop();

    } // while (true)

    return 0;
}
