#include "cons.h"

#include "vga.h"
#include "graphics.h"
#include "ps2.h"
#include <string.h>
#include <stdarg.h>
#include <pico/stdlib.h>
#include <pico/platform.h>

uint8_t cons_buffer[TEXTMODE_COLS * 24] ;
#define TS_NORMAL      0
#define TS_WAITBRACKET 1
#define TS_STARTCHAR   2 
#define TS_READPARAM   3
#define TS_HASH        4
#define TS_READCHAR    5

static uint8_t terminal_state = TS_NORMAL ;
static int cursor_col = 0, cursor_row = 3, saved_col = 0, saved_row = 0;
static int scroll_region_start = 3, scroll_region_end = 26 ;
static bool cursor_shown = true ;
static uint8_t color_fg = 7, color_bg = 0, attr = 0 ;

uint8_t keyboard_map_key_ascii(uint8_t key, uint8_t modifier, bool *isaltcode) ;

char keyboard_input  ;

void __time_critical_func(handle_ps2)(uint8_t key, uint8_t modifier) {
        uint8_t ascii = keyboard_map_key_ascii(key, modifier, NULL) ;
        keyboard_input = ascii ;
}

void cons_cls() {
    memset(char_buffer + TEXTMODE_COLS * 3, ' ', TEXTMODE_COLS * 24) ;
    memset(attr_buffer + TEXTMODE_COLS * 3, 0 << 4 | 7 & 0x0F, TEXTMODE_COLS * 24) ;
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

static void cons_scroll_region(uint8_t top_limit, uint8_t bottom_limit, int8_t offset, uint8_t color_fg, uint8_t color_bg) {
    if (offset == 0) {
        return ;
    }

    uint8_t rows = bottom_limit - top_limit + 1 ; 
    
    // scroll up
    if (offset > 0) {
        memcpy(cons_buffer, &char_buffer[TEXTMODE_COLS * (top_limit + offset)], TEXTMODE_COLS * (rows - offset)) ;
        memcpy(&char_buffer[TEXTMODE_COLS * top_limit], cons_buffer, TEXTMODE_COLS * (rows - offset)) ;
        memset(&char_buffer[TEXTMODE_COLS * (bottom_limit - offset + 1)], ' ', TEXTMODE_COLS * offset) ;
        memset(&attr_buffer[TEXTMODE_COLS * (bottom_limit - offset + 1)], color_bg << 4 | color_fg, TEXTMODE_COLS * offset) ;
        return ;
    }

    // scroll down

    memcpy(cons_buffer, &char_buffer[TEXTMODE_COLS * top_limit], TEXTMODE_COLS * (rows + offset)) ;
    memcpy(&char_buffer[TEXTMODE_COLS * (top_limit - offset)], cons_buffer, TEXTMODE_COLS * (rows + offset)) ;
    memset(&char_buffer[TEXTMODE_COLS * top_limit], ' ', TEXTMODE_COLS * -offset) ;
    memset(&attr_buffer[TEXTMODE_COLS * top_limit], color_bg << 4 | color_fg, TEXTMODE_COLS * -offset) ;
}

static uint8_t cons_get_attribute(uint8_t x, uint8_t y, uint8_t attr) {
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

static void cons_set_attribute(uint8_t x, uint8_t y, uint8_t attr, uint8_t val) {
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

static void cons_show_cursor(bool show) {
    cons_set_attribute(cursor_col, cursor_row, ATTR_BGCOLOR, show ? 7 : 0) ;
    cursor_shown = show ;
}

static void move_cursor_wrap(int row, int col) {
    if (row == cursor_row && col == cursor_col) {
        return ;
    }

    int top_limit    = scroll_region_start ;
    int bottom_limit = scroll_region_end ;
    
    if (cursor_shown && cursor_row >= 0 && cursor_col >= 0) {
        cons_show_cursor(false) ;
    }
    
    while (col < 0) {
        col += TEXTMODE_COLS ;
        row--;
    }
    
    while (row < top_limit) {
        row++;
        cons_scroll_region(top_limit, bottom_limit, -1, color_fg, color_bg) ;
    }
    while (col >= TEXTMODE_COLS) {
        col -= TEXTMODE_COLS ;
        row++ ;
    }
    while (row > bottom_limit) {
        row--;
        cons_scroll_region(top_limit, bottom_limit, 1, color_fg, color_bg) ;
    }

    cursor_row = row ;
    cursor_col = col ;
    
    cons_show_cursor(true) ;
}

static void move_cursor_within_region(int row, int col, int top_limit, int bottom_limit) {
    if (row == cursor_row && col == cursor_col) {
        return ;
    }

    if (cursor_shown && cursor_row >= 0 && cursor_col >= 0) {
        cons_show_cursor(false) ;
    }

    if (col < 0) {
        col = 0 ;
    } else if (col >= TEXTMODE_COLS) {
        col = TEXTMODE_COLS - 1 ;
    }

    if (row < top_limit) {
        row = top_limit ;
    } else if (row > bottom_limit) {
        row = bottom_limit ;
    }
        
          
    cursor_row = row ;
    cursor_col = col ;

    cons_show_cursor(true) ;
}

static void move_cursor_limited(int row, int col) {
  // only move if cursor is currently within scroll region, do not move
  // outside of scroll region
    if (cursor_row >= scroll_region_start && cursor_row <= scroll_region_end) {
        move_cursor_within_region(row, col, scroll_region_start, scroll_region_end) ;
    }
}

static void init_cursor(int row, int col) {
    cursor_row = -1;
    cursor_col = -1;
    move_cursor_within_region(row, col, 0, 26) ;
}

static void cons_put_char(char c) {
    int addr = TEXTMODE_COLS * cursor_row + cursor_col ;
    char_buffer[addr] = c;
    attr_buffer[addr] = color_bg << 4 | color_fg & 0x0F ;
    init_cursor(cursor_row, cursor_col + 1) ;
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

static void cons_process_text(char c) {
    switch (c) {
        case 5: // ENQ => send answer-back string
        // send_string("murm-pdp-11") ;
        break;
      
        case 7: // BEL => produce beep
        //   sound_play_tone(config_get_audible_bell_frequency(), 
        //                   config_get_audible_bell_duration(), 
        //                   config_get_audible_bell_volume(), 
        //                   false);
        //   framebuf_flash_screen(config_get_visual_bell_color(), config_get_visual_bell_duration());
            graphics_set_flashmode(false, true) ;
            break ;
      
        case 8:   // backspace
        case 127: { // delete
            uint8_t mode = c == 8 ? 1 : 2 ;
            if (mode > 0) {
                int top_limit = scroll_region_start ;
                if (cursor_row > top_limit) {
                    move_cursor_wrap(cursor_row, cursor_col - 1) ;
                } else {
                    move_cursor_limited(cursor_row, cursor_col - 1) ;
                }

                if (mode == 2) {
                    char_buffer[TEXTMODE_COLS * cursor_row + cursor_col] = ' ' ;
                    cons_set_attribute(cursor_col, cursor_row, ATTR_BGCOLOR, 0) ;
                    cons_show_cursor(cursor_shown) ;
                }
            }

            break;
        }

    case '\t': // horizontal tab
      {
        int col = cursor_col + 4 ;
        move_cursor_limited(cursor_row, col) ;
        break;
      }
      
    case '\n': // newline
    case 11:   // vertical tab (interpreted as newline)
        move_cursor_wrap(cursor_row + 1, cursor_col) ;
        break ;
    case 12:   // form feed (interpreted as newline)
    case '\r': // carriage return
        move_cursor_wrap(cursor_row, 0) ;
        break ;

    case 14:  // SO
      //charset = &charset_G1; 
        break;

    case 15:  // SI
      //charset = &charset_G0; 
        break;

    default: // regular character
        if (c >= 32 ) {
            cons_put_char(c) ;
        }
        break;
    }
}


void cons_put_char_vt100(char c) {
    static char    start_char = 0 ;
    static uint8_t num_params = 0 ;
    static uint8_t params[16] ;

    if (terminal_state != TS_NORMAL) {
        if (c == 8 || c == 10 || c == 13) {
            // processe some cursor control characters within escape sequences
            // (otherwise we fail "vttest" cursor control tests)
            cons_process_text(c);
            return;
        } else if (c == 11) {
            // ignore VT character plus the following character
            // (otherwise we fail "vttest" cursor control tests)
            terminal_state = TS_READCHAR ;
            return;
        }
    }

    switch (terminal_state) {
        case TS_NORMAL: {
            if (c == 27) {
                terminal_state = TS_WAITBRACKET ;
            } else {
                cons_process_text(c) ;
            }

            break;
        }

        case TS_WAITBRACKET: {
            terminal_state = TS_NORMAL;

            switch (c) {
                case '[':
                    start_char = 0;
                    num_params = 1;
                    params[0] = 0;
                    terminal_state = TS_STARTCHAR;
                    break;
            
                case '#':
                    terminal_state = TS_HASH;
                    break;
            
                case  27: cons_put_char(c); break;                           // escaped ESC
                // case 'c': terminal_reset(); break;                           // reset
                // case '7': terminal_process_command(0, 's', 0, NULL); break;  // save cursor position
                // case '8': terminal_process_command(0, 'u', 0, NULL); break;  // restore cursor position
                // case 'H': tabs[cursor_col] = true; break;                    // set tab
                // case 'J': terminal_process_command(0, 'J', 0, NULL); break;  // clear to end of screen
                // case 'K': terminal_process_command(0, 'K', 0, NULL); break;  // clear to end of row
                case 'D': move_cursor_wrap(cursor_row+1, cursor_col); break; // cursor down
                case 'E': move_cursor_wrap(cursor_row+1, 0); break;          // cursor down and to first column
                case 'I': move_cursor_wrap(cursor_row-1, 0); break;          // cursor up and to furst column
                case 'M': move_cursor_wrap(cursor_row-1, cursor_col); break; // cursor up
                case '(': 
                case ')': 
                case '+':
                case '*':
                    start_char = c;
                    terminal_state = TS_READCHAR;
                    break;
            }

            break;
        }

    case TS_STARTCHAR:
    case TS_READPARAM: {
        if (c >= '0' && c <= '9') {
            params[num_params - 1] = params[num_params - 1] * 10 + (c - '0') ;
            terminal_state = TS_READPARAM;
        } else if (c == ';') {
            // next parameter (max 16 parameters)
            num_params++ ;
            if (num_params > 16) {
                terminal_state = TS_NORMAL;
            } else {
                params[num_params-1] = 0 ;
                terminal_state = TS_READPARAM;
            }
        } else if (terminal_state == TS_STARTCHAR && (c == '?' || c == '#')) {
            start_char = c;
            terminal_state = TS_READPARAM;
        } else {
            // not a parameter value or startchar => command is done
            
            // terminal_process_command(start_char, c, num_params, params);
            terminal_state = TS_NORMAL;
        }
        
        break;
    }

    case TS_HASH: {
        switch (c) {
            case '3': {
                // framebuf_set_row_attr(cursor_row, ROW_ATTR_DBL_WIDTH | ROW_ATTR_DBL_HEIGHT_TOP);
                break;
            }

            case '4': {
                // framebuf_set_row_attr(cursor_row, ROW_ATTR_DBL_WIDTH | ROW_ATTR_DBL_HEIGHT_BOT);
                break;
            }
            
            case '5': {
                // framebuf_set_row_attr(cursor_row, 0) ;
                break;
            }

            case '6': {
                // framebuf_set_row_attr(cursor_row, ROW_ATTR_DBL_WIDTH);
                break;
            }

            case '8': {
                // fill screen with 'E' characters (DEC test feature)
                int top_limit    = scroll_region_start ;
                int bottom_limit = scroll_region_end ;
                cons_show_cursor(false);
                // framebuf_fill_region(0, top_limit, framebuf_get_ncols(-1)-1, bottom_limit, 'E', color_fg, color_bg);
                // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
                cons_show_cursor(true);
                break;
            }
        }
        
        terminal_state = TS_NORMAL;
        break;
    }

    case TS_READCHAR: {
        if (start_char=='(') {
            // charset_G0 = get_charset(c);
        } else if (start_char==')') {
            // charset_G1 = get_charset(c);
        }

        terminal_state = TS_NORMAL;
        break;
      }
    }
}
