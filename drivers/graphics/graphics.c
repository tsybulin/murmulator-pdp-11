#include "graphics.h"
#include <string.h>
#include <stdarg.h>

void draw_text(const char string[TEXTMODE_COLS + 1], uint32_t x, uint32_t y, uint8_t color, uint8_t bgcolor) {
    uint8_t* t_buf = text_buffer + TEXTMODE_COLS * 2 * y + 2 * x;
    for (int xi = TEXTMODE_COLS * 2; xi--;) {
        if (!*string) break;
        *t_buf++ = *string++;
        *t_buf++ = bgcolor << 4 | color & 0xF;
    }
}

void draw_window(const char title[TEXTMODE_COLS + 1], uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    char line[width + 1];
    memset(line, 0, sizeof line);
    width--;
    height--;
    // Рисуем рамки

    memset(line, 0xCD, width); // ═══


    line[0] = 0xC9; // ╔
    line[width] = 0xBB; // ╗
    draw_text(line, x, y, 11, 1);

    line[0] = 0xC8; // ╚
    line[width] = 0xBC; //  ╝
    draw_text(line, x, height + y, 11, 1);

    memset(line, ' ', width);
    line[0] = line[width] = 0xBA;

    for (int i = 1; i < height; i++) {
        draw_text(line, x, y + i, 11, 1);
    }

    snprintf(line, width - 1, " %s ", title);
    draw_text(line, x + (width - strlen(line)) / 2, y, 14, 3);
}

void gprintf(const char *__restrict format, ...) {
    char tmp[TEXTMODE_COLS + 1] ;
    va_list args ;
    va_start(args, format) ;
    vsnprintf(tmp, TEXTMODE_COLS, format, args) ;
    draw_text(tmp, 0, 1, 12, 0) ;
}

int term_pos_col = 0 ;
int term_pos_row = 3 ;

void cons_put_char(char c) {
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

uint8_t cons_buffer[TEXTMODE_COLS * 24 * 2] ;

void cons_init() {
    memset(&cons_buffer[TEXTMODE_COLS * 23 * 2], 0, TEXTMODE_COLS * 2) ;
}

void cons_scroll() {
    memcpy(cons_buffer, &text_buffer[TEXTMODE_COLS * 4 * 2], TEXTMODE_COLS * 23 * 2) ;
    memcpy(&text_buffer[TEXTMODE_COLS * 3 * 2], cons_buffer, TEXTMODE_COLS * 24 * 2) ;
}