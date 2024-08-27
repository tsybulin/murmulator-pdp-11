#include <cstdlib>
#include <cstring>
#include <hardware/clocks.h>
#include <hardware/structs/vreg_and_chip_reset.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>

#include "graphics.h"
#include "ff.h"

extern "C" {
#include "ps2.h"
#include "cons.h"
}

int startup( char *rkfile, char *rlfile, int bootdev) ;

static FATFS fs ;
uint8_t SCREEN[TEXTMODE_ROWS * TEXTMODE_COLS] ;
uint8_t ATTRS[TEXTMODE_ROWS * TEXTMODE_COLS] ;
semaphore vga_start_semaphore ;

constexpr char BASEDIR[] = "PDP-11" ;

#define frame_tick (16666)

void __time_critical_func(render_core)() {
    multicore_lockout_victim_init();
    graphics_init();
    graphics_set_bgcolor(0x000000);
    graphics_set_offset(0, 0);
    graphics_set_mode(TEXTMODE_DEFAULT);
    graphics_set_buffer(SCREEN, TEXTMODE_COLS, TEXTMODE_ROWS);
    graphics_set_textbuffer(SCREEN, ATTRS);
    clrScr(0);

    sem_acquire_blocking(&vga_start_semaphore);
    // 60 FPS loop
    uint64_t tick = time_us_64();
    uint64_t last_input_tick = tick;
    while (true) {
        // Every 60th frame
        if (tick >= last_input_tick + frame_tick * 60) {
            gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN));
            last_input_tick = tick;
        }
        tick = time_us_64();

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

    gpio_put(PICO_DEFAULT_LED_PIN, true);

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);
    
    sleep_ms(250) ;

    cons_init() ;

    cons_draw_line("Murmulator PDP-11", 2) ;
    cons_draw_line(nullptr, 27) ;

    if (FR_OK != f_mount(&fs, "SD", 1)) {
        gprintf("SD Card not inserted or SD Card error!");
        while (true);
    }

    DIR dir ;

    if (FR_OK != f_opendir(&dir, BASEDIR)) {
        gprintf("Failed to open directory %s", BASEDIR);
        while (true);
    }

    char rkfile[] = "PDP-11/BOOT.RK05" ;
    char rlfile[] = "PDP-11/WORK.RL02" ;

    startup(rkfile, rlfile, 0) ;
    // while(1) ;
    
    __unreachable() ;
}
