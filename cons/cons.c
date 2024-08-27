#include "cons.h"

#include "vga.h"
#include "graphics.h"
#include "ps2.h"
#include <string.h>
#include <stdarg.h>
#include <pico/stdlib.h>
#include <pico/platform.h>

uint8_t cons_buffer[TEXTMODE_COLS * 24] ;
int term_pos_col = 0 ;
int term_pos_row = 3 ;

uint8_t keyboard_map_key_ascii(uint8_t key, uint8_t modifier, bool *isaltcode) ;

char keyboard_input  ;

void __time_critical_func(handle_ps2)(uint8_t key, uint8_t modifier) {
        uint8_t ascii = keyboard_map_key_ascii(key, modifier, NULL) ;
        keyboard_input = ascii ;
}

void cons_cls() {
    memset(char_buffer + TEXTMODE_COLS * 2, ' ', TEXTMODE_COLS * 24) ;
    memset(attr_buffer + TEXTMODE_COLS * 2, 0 << 4 | 7 & 0x0F, TEXTMODE_COLS * 24) ;
}

void cons_init() {
    memset(cons_buffer, ' ', TEXTMODE_COLS * 24) ;
    cons_cls() ;
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
    memcpy(cons_buffer, &char_buffer[TEXTMODE_COLS * 4], TEXTMODE_COLS * 23) ;
    memcpy(&char_buffer[TEXTMODE_COLS * 3], cons_buffer, TEXTMODE_COLS * 24) ;
}

uint8_t cons_get_attribute(uint8_t x, uint8_t y, uint8_t attr) {
    uint8_t* t_buf = attr_buffer + TEXTMODE_COLS * y + x ;
    if (attr == ATTR_COLOR) {
        return *t_buf & 0x0F ;
    }

    if (attr == ATTR_BGCOLOR) {
        return (*t_buf & 0xF0) >> 4 ;
    }

    gprintf("cons_get_attribute: unknown attr 0x%02X", attr) ;

    return 0 ;
}

void cons_set_attribute(uint8_t x, uint8_t y, uint8_t attr, uint8_t val) {
    uint8_t* t_buf = attr_buffer + TEXTMODE_COLS * y + x ;
    if (attr == ATTR_COLOR) {
        *t_buf = (*t_buf & 0xF0) | val ;
        return ;
    }

    if (attr == ATTR_BGCOLOR) {
        *t_buf = (*t_buf & 0x0F) | (val << 4) ;
        return ;
    }

    gprintf("cons_set_attribute: unknown attr 0x%02X", attr) ;
}

void cons_put_char(char c) {
    if (term_pos_col >= 0 && term_pos_col < TEXTMODE_COLS && term_pos_row > 2 && term_pos_row < 27) {
        cons_set_attribute(term_pos_col, term_pos_row, ATTR_BGCOLOR, 0) ;
    }

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
        if (++term_pos_row > 26) {
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
        if (++term_pos_row > 26) {
            term_pos_row = 26 ;
            cons_scroll() ;
        }
    }

    int addr = TEXTMODE_COLS * term_pos_row + term_pos_col ;
    char_buffer[addr] = c;
    attr_buffer[addr] = 0 << 4 | 7 & 0xF;

    term_pos_col++ ;

    if (term_pos_col >= 0 && term_pos_col < TEXTMODE_COLS && term_pos_row > 2 && term_pos_row < 27) {
        cons_set_attribute(term_pos_col, term_pos_row, ATTR_BGCOLOR, 7) ;
    }
}

void cons_printf(const char *__restrict format, ...) {
    char tmp[TEXTMODE_COLS + 1] ;
    va_list args ;
    va_start(args, format) ;
    vsnprintf(tmp, TEXTMODE_COLS, format, args) ;
    
    char *p = tmp ;
    for (int xi = TEXTMODE_COLS; xi--;) {
        if (!*p) break ;
        cons_put_char(*p++) ;
    }
}

#define TS_NORMAL      0
#define TS_WAITBRACKET 1
#define TS_STARTCHAR   2 
#define TS_READPARAM   3
#define TS_HASH        4
#define TS_READCHAR    5

// static uint8_t terminal_state = TS_NORMAL ;
// static int cursor_col = 0, cursor_row = 0, saved_col = 0, saved_row = 0;
// static int scroll_region_start, scroll_region_end ;
// static bool cursor_shown = true ;
// static uint8_t color_fg, color_bg, attr = 0, cur_attr = 0;

// static void show_cursor(bool show) {
//     uint8_t attr = ATTR_BLINK ;
//     text_buffer[]
//   framebuf_set_attr(cursor_col, cursor_row, show ? (cur_attr ^ attr) : cur_attr);
// }


// static void move_cursor_wrap(int row, int col) {
//     if (row != cursor_row || col != cursor_col) {
//         int top_limit    = scroll_region_start ;
//         int bottom_limit = scroll_region_end ;
      
//         if (cursor_shown && cursor_row >= 0 && cursor_col >= 0) {
//             show_cursor(false) ;
//         }
      
//       while( col<0 )                        { col += framebuf_get_ncols(row); row--; }
//       while( row<top_limit )                { row++; framebuf_scroll_region(top_limit, bottom_limit, -1, color_fg, color_bg); }
//       while( col>=framebuf_get_ncols(row) ) { col -= framebuf_get_ncols(row); row++; }
//       while( row>bottom_limit )             { row--; framebuf_scroll_region(top_limit, bottom_limit, 1, color_fg, color_bg); }

//       cursor_row = row;
//       cursor_col = col;
      
//       cur_attr = framebuf_get_attr(cursor_col, cursor_row);
//       if( cursor_shown ) show_cursor(true);
//     }
// }


// void cons_put_char_vt100(char c) {
//     static char    start_char = 0 ;
//     static uint8_t num_params = 0 ;
//     static uint8_t params[16] ;


// }

