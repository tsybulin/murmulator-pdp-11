#include "graphics.h"
#include <string.h>
#include <stdarg.h>

void draw_text(const char string[TEXTMODE_COLS + 1], uint32_t x, uint32_t y, uint8_t color, uint8_t bgcolor) {
    uint8_t* c_buf = char_buffer + TEXTMODE_COLS * y + x;
    uint8_t* a_buf = attr_buffer + TEXTMODE_COLS * y + x;
    for (int xi = TEXTMODE_COLS; xi--;) {
        if (!*string) break;
        *c_buf++ = *string++;
        *a_buf++ = bgcolor << 4 | color & 0xF;
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
    draw_text(line, x, y, 6, 0);

    line[0] = 0xC8; // ╚
    line[width] = 0xBC; //  ╝
    draw_text(line, x, height + y, 6, 0);

    memset(line, ' ', width);
    line[0] = line[width] = 0xBA;

    for (int i = 1; i < height; i++) {
        draw_text(line, x, y + i, 6, 0);
    }

    snprintf(line, width - 1, " %s ", title);
    draw_text(line, x + (width - strlen(line)) / 2, y, 14, 8);
}

void graph_draw_line(const char* title, uint32_t row) {
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

void lgprintf(uint32_t x, uint32_t y, uint8_t color, uint8_t bgcolor, const char *__restrict format, ...) {
    char tmp[TEXTMODE_COLS + 1] ;
    va_list args ;
    va_start(args, format) ;
    vsnprintf(tmp, TEXTMODE_COLS, format, args) ;
    draw_text(tmp, x, y, color, bgcolor) ;
}
