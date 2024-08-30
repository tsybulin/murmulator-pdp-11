#include <cstdlib>
#include <cstring>
#include <hardware/clocks.h>
#include <hardware/structs/vreg_and_chip_reset.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include "pico/util/queue.h"
#include "graphics.h"
#include "ff.h"
#include "ini.h"

extern "C" {
#include "ps2.h"
#include "cons.h"
}

typedef struct configuration {
    const char* name;
    const char* rk;
    const char* rl;
} configuration_t ;

static configuration_t configurations[5] = {
    {
        "Built-in",
        "PDP-11/BOOT.RK05",
        "PDP-11/WORK.RL02"
    }
} ;

static int config_handler(void* user, const char* section, const char* name, const char* value) {
    int c = -1 ;

    if (*section && strlen(section) > 0) {
        for (int i = 0; i < 5; i++) {
            if (strcmp(configurations[i].name, section) == 0) {
                c = i ;
                break;
            }
        }

        if (c < 0) {
            for (int i = 0; i < 5; i++) {
                if (strlen(configurations[i].name) > 0) {
                    continue;
                }

                c = i ;
                configurations[i].name = strdup(section) ;
                break;
            }

            if (c < 0) {
                gprintf("too many configurations") ;
                while(1) ;
                return 0 ;
            }
        }

        if (strcmp(name, "RK") == 0) {
            configurations[c].rk = strdup(value) ;
        } else if (strcmp(name, "RL") == 0) {
            configurations[c].rl = strdup(value) ;
        }

        return 1;
    } else {
        gprintf("empty config section") ;
        while(1) ;
        return 0 ;
    }
}

int startup( char *rkfile, char *rlfile, int bootdev) ;

static FATFS fs ;
uint8_t SCREEN[TEXTMODE_ROWS * TEXTMODE_COLS] ;
uint8_t ATTRS[TEXTMODE_ROWS * TEXTMODE_COLS] ;
semaphore vga_start_semaphore ;
constexpr char BASEDIR[] = "PDP-11" ;
volatile uint64_t led_offtime = 0 ;
extern queue_t keyboard_queue ;

void __time_critical_func(render_core)() {
    multicore_lockout_victim_init();
    graphics_init();
    graphics_set_bgcolor(0x000000);
    graphics_set_offset(0, 0);
    graphics_set_mode(TEXTMODE_DEFAULT);
    graphics_set_buffer(SCREEN, TEXTMODE_COLS, TEXTMODE_ROWS);
    graphics_set_textbuffer(SCREEN, ATTRS);
    clrScr(0);

    sem_acquire_blocking(&vga_start_semaphore) ;

    while (true) {
        if (led_offtime > 0 && to_us_since_boot(get_absolute_time()) >= led_offtime) {
            gpio_put(PICO_DEFAULT_LED_PIN, false) ;
            led_offtime = 0 ;
        }
        tight_loop_contents();
    }

    __unreachable();
}

int main() {
    set_sys_clock_khz(200000, true) ;
    busy_wait_ms(1000) ;

    stdio_uart_init_full(PIN_UART_ID, 115200, PIN_UART_TX, PIN_UART_RX) ;

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);
    
    sleep_ms(250) ;

    cons_init() ;

    graph_draw_line("Murmulator PDP-11", 2) ;
    graph_draw_line(nullptr, 27) ;

    if (FR_OK != f_mount(&fs, "SD", 1)) {
        gprintf("SD Card not inserted or SD Card error!");
        while (true);
    }

    if (FRESULT res = ini_parse("PDP-11/CONFIG.INI", config_handler, NULL); res != FR_OK) {
        gprintf("Can't load 'PDP-11/CONFIG.INI' err %d", res);
        while (true);
    }

    draw_window("BOOT         ", 15, 6, 50, 9) ;
    for (int i = 0; i < 5; i++) {
        char tmp[20] ;
        snprintf(tmp, 20, "%d. %s", i, configurations[i].name) ;
        draw_text(tmp, 20, 7 + i, 6, 0) ;
    }

    draw_text("Press a number of configuration to boot", 20, 13, 6, 0) ;

    char rkfile[50] = "PDP-11/BOOT.RK05" ;
    char rlfile[50] = "PDP-11/WORK.RL02" ;

    int ci = 0 ;
    static uint64_t cfg_offtime = to_us_since_boot(make_timeout_time_ms(10000)) ;
    while (to_us_since_boot(get_absolute_time()) < cfg_offtime) {
        if (!queue_is_empty(&keyboard_queue)) {
            cfg_offtime = to_us_since_boot(make_timeout_time_ms(30000)) ;
            uint8_t c  = 0 ;
            if (queue_try_remove(&keyboard_queue, &c)) {
                if (c == '0') {
                    ci = 0 ;
                    break ;
                } else if (c == '1') {
                    ci = 1 ;
                    break ;
                } else if (c == '2') {
                    ci = 2 ;
                    break ;
                } else if (c == '3') {
                    ci = 3 ;
                    break ;
                } else if (c == '4') {
                    ci = 4 ;
                    break ;
                } else if (c == '\n' || c == '\r') {
                    ci = 0 ;
                    break;
                }
            }
        }

        char tmp[11] ;
        snprintf(tmp, 10, "in %2llu s.", (cfg_offtime - to_us_since_boot(get_absolute_time())) / 1000000U) ;
        draw_text(tmp, 38, 6, 6, 0) ;
        busy_wait_ms(250) ;
    }

    strncpy(rkfile, configurations[ci].rk, 50) ;
    strncpy(rlfile, configurations[ci].rl, 50) ;
    cons_cls() ;

    startup(rkfile, rlfile, 0) ;
    // while(1) ;
    
    __unreachable() ;
}
