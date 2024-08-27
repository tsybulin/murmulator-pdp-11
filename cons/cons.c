#include "cons.h"

#include "vga.h"
#include "graphics.h"
#include "ps2.h"
#include <string.h>
#include <stdarg.h>
#include <pico/stdlib.h>
#include <pico/platform.h>

uint8_t cons_buffer[TEXTMODE_COLS * 24 * 2] ;
int term_pos_col = 0 ;
int term_pos_row = 3 ;

uint8_t keyboard_map_key_ascii(uint8_t key, uint8_t modifier, bool *isaltcode) ;

char keyboard_input  ;

void __time_critical_func(handle_ps2)(uint8_t key, uint8_t modifier) {
        uint8_t ascii = keyboard_map_key_ascii(key, modifier, NULL) ;
        keyboard_input = ascii ;
}

void cons_init() {
    memset(&cons_buffer[TEXTMODE_COLS * 23 * 2], 0, TEXTMODE_COLS * 2) ;
    keyboard_init(handle_ps2) ;
}

void cons_draw_line(const char* title, uint32_t row) {
    char line[TEXTMODE_COLS + 1] ;
    memset(line, 0, sizeof line) ;

    memset(line, title ? 0xCD : 0xC4, TEXTMODE_COLS) ; // ═ : ─
    draw_text(line, 0, row, 7, 0);

    if (!title) {
        return ;
    }

    snprintf(line, TEXTMODE_COLS, " %s ", title);
    draw_text(line, (TEXTMODE_COLS - strlen(line)) / 2, row, 2, 0);
}

void cons_scroll() {
    memcpy(cons_buffer, &text_buffer[TEXTMODE_COLS * 4 * 2], TEXTMODE_COLS * 23 * 2) ;
    memcpy(&text_buffer[TEXTMODE_COLS * 3 * 2], cons_buffer, TEXTMODE_COLS * 24 * 2) ;
}

void cons_put_char(char c) {
    if (c == 0x08) {
        if (term_pos_col > 0) {
            term_pos_col-- ;
        }
        return ;
    }

    if (c == '\n') {
        term_pos_col = 0 ;
        return ;
    }

    if (c == '\r') {
        if (++term_pos_row >= 27) {
            term_pos_row = 26 ;
            cons_scroll() ;
        }
        return ;
    }

    if (c == '\t') {
        cons_put_char(' ') ;
        cons_put_char(' ') ;
        cons_put_char(' ') ;
        cons_put_char(' ') ;
    }

    if (term_pos_col >= TEXTMODE_COLS) {
        term_pos_col = 0 ;
        if (++term_pos_row >= 27) {
            term_pos_row = 26 ;
            cons_scroll() ;
        }
    }

    uint8_t* t_buf = text_buffer + TEXTMODE_COLS * 2 * term_pos_row + 2 * term_pos_col ;
    *t_buf++ = c;
    *t_buf++ = 0 << 4 | 7 & 0xF;

    term_pos_col++ ;
}

void cons_printf(const char *__restrict format, ...) {
    char tmp[TEXTMODE_COLS + 1] ;
    va_list args ;
    va_start(args, format) ;
    vsnprintf(tmp, TEXTMODE_COLS, format, args) ;
    
    char *p = tmp ;
    for (int xi = TEXTMODE_COLS * 2; xi--;) {
        if (!*p) break ;
        cons_put_char(*p++) ;
    }
}
