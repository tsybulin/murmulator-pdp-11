#include "cons.h"

#include "vga.h"
#include "graphics.h"
#include "ps2.h"
#include <string.h>
#include <stdarg.h>
#include <pico/stdlib.h>
#include <pico/platform.h>
#include "pico/util/queue.h"
#include "hardware/watchdog.h"

#define CONS_TOP    3
#define CONS_BOTTOM 26
#define CONS_ROWS   24
// TEXTMODE_COLS * CONS_ROWS
#define CONS_SIZE   1920
#define CONS_LAST_ROW 23
#define CONS_ATTR(fg, bg) (bg << 4 | fg & 0x0F)
#define BUFFADDR(addr) (TEXTMODE_COLS * (addr))
#define CONSADDR(addr) (TEXTMODE_COLS * (CONS_TOP + addr))
#define BUFSIZE(sz) (TEXTMODE_COLS * (sz))

uint8_t cons_buffer[CONS_SIZE] ;
#define TS_NORMAL      0
#define TS_WAITBRACKET 1
#define TS_STARTCHAR   2 
#define TS_READPARAM   3
#define TS_HASH        4
#define TS_READCHAR    5

#define COLOR_FG 6

// charset
#define CS_TEXT     0
#define CS_GRAPHICS 2

#define ATTR_UNDERLINE 0x01
#define ATTR_BLINK     0x02
#define ATTR_BOLD      0x04
#define ATTR_INVERSE   0x08

static uint8_t terminal_state = TS_NORMAL ;
static int cursor_col = 0, cursor_row = 0, saved_col = 0, saved_row = 0;
static int scroll_region_start = 0, scroll_region_end = CONS_LAST_ROW ;
static bool cursor_shown = true, cursor_eol = false, saved_eol = false ;
static uint8_t color_fg = COLOR_FG, color_bg = 0, attr = 0 ;
static uint8_t saved_attr, saved_fg, saved_bg, saved_charset_G0, saved_charset_G1, *charset, charset_G0, charset_G1 ;

uint8_t keyboard_map_key_ascii(uint8_t key, uint8_t modifier, bool *isaltcode) ;

static queue_t ps2_rx_queue ;
queue_t keyboard_queue ;

static void cons_send_char(char c) {
    queue_try_add(&keyboard_queue, &c) ;
}

static void cons_send_string(const char *str) {
    while (*str) {
        queue_try_add(&keyboard_queue, str++) ;
    }
}

static void cons_send_escape_sequence(uint8_t key) {
    cons_send_string("\033[") ;
    cons_send_char(key) ;
}

static void cons_process_keyboard_vt(uint8_t key) {
    switch (key) {
        case KEY_UP:    cons_send_escape_sequence('A') ; break ;
        case KEY_DOWN:  cons_send_escape_sequence('B') ; break ;
        case KEY_RIGHT: cons_send_escape_sequence('C') ; break ;
        case KEY_LEFT:  cons_send_escape_sequence('D') ; break ;

        case KEY_F1:
        case KEY_F2:
        case KEY_F3:
            cons_send_string("\033O") ;
            cons_send_char('P' + (key - KEY_F1)) ;
            break;

        case KEY_F5:
            cons_send_string("\033Ow") ;
            break;

        case HID_KEY_RETURN:
            cons_send_string("\033OM") ;
            break;

        default:
            cons_send_char(key) ;
            break;
    }
}

extern uint64_t led_offtime ;

void __time_critical_func(handle_ps2)(uint8_t key, uint8_t modifier) {
    if (modifier & KEYBOARD_MODIFIER_LEFTALT && modifier & KEYBOARD_MODIFIER_LEFTCTRL && key == HID_KEY_DELETE) {
        watchdog_enable(10, false) ;
        while(1) ;
    }
    gpio_put(PICO_DEFAULT_LED_PIN, true) ;
    led_offtime = to_us_since_boot(make_timeout_time_ms(50)) ;
    uint8_t ascii = keyboard_map_key_ascii(key, modifier, NULL) ;
    cons_process_keyboard_vt(ascii) ;
}

void cons_cls() {
    memset(char_buffer + BUFFADDR (CONS_TOP), ' ', CONS_SIZE) ;
    memset(attr_buffer + BUFFADDR (CONS_TOP), CONS_ATTR(color_fg, color_bg), CONS_SIZE) ;
}

void cons_init() {
    memset(cons_buffer, ' ', CONS_SIZE) ;
    cons_cls() ;
    queue_init(&ps2_rx_queue, 1, 32) ;
    queue_init(&keyboard_queue, 1, 32) ;
    keyboard_init(handle_ps2) ;

    charset_G0 = CS_TEXT;
    charset_G1 = CS_GRAPHICS;
    saved_charset_G0 = CS_TEXT;
    saved_charset_G1 = CS_GRAPHICS;
    charset = &charset_G0;
}

static void cons_scroll_region(uint8_t top_limit, uint8_t bottom_limit, int8_t offset, uint8_t color_fg, uint8_t color_bg) {
    if (offset == 0) {
        return ;
    }

    uint8_t rows = bottom_limit - top_limit + 1 ; 
    
    // scroll up
    if (offset > 0) {
        memcpy(cons_buffer, &char_buffer[CONSADDR (top_limit + offset)], BUFSIZE (rows - offset)) ;
        memcpy(&char_buffer[CONSADDR (top_limit)], cons_buffer, BUFSIZE (rows - offset)) ;
        memset(&char_buffer[CONSADDR (bottom_limit - offset + 1)], ' ', BUFSIZE (offset)) ;
        memset(&attr_buffer[CONSADDR (bottom_limit - offset + 1)], CONS_ATTR(color_fg, color_bg), BUFSIZE (offset)) ;
        return ;
    }

    // scroll down

    memcpy(cons_buffer, &char_buffer[CONSADDR (top_limit)], BUFSIZE (rows + offset)) ;
    memcpy(&char_buffer[CONSADDR (top_limit - offset)], cons_buffer, BUFSIZE (rows + offset)) ;
    memset(&char_buffer[CONSADDR (top_limit)], ' ', BUFSIZE (-offset)) ;
    memset(&attr_buffer[CONSADDR (top_limit)], color_bg << 4 | color_fg, BUFSIZE (-offset)) ;
}

static uint8_t cons_get_attribute(uint8_t x, uint8_t y, uint8_t attr) {
    uint8_t* a_buf = attr_buffer + CONSADDR (y) + x ;
    if (attr == ATTR_COLOR) {
        return *a_buf & 0x0F ;
    }

    if (attr == ATTR_BGCOLOR) {
        return (*a_buf & 0xF0) >> 4 ;
    }

    gprintf("cons_get_attribute: unknown attr 0x%02X", attr) ;

    return 0 ;
}

static void cons_set_attribute(uint8_t x, uint8_t y, uint8_t attr, uint8_t val) {
    uint8_t* a_buf = attr_buffer + CONSADDR (y) + x ;
    if (attr == ATTR_COLOR) {
        *a_buf = (*a_buf & 0xF0) | val ;
        return ;
    }

    if (attr == ATTR_BGCOLOR) {
        *a_buf = (*a_buf & 0x0F) | (val << 4) ;
        return ;
    }

    gprintf("cons_set_attribute: unknown attr 0x%02X", attr) ;
}

static void cons_show_cursor(bool show) {
    uint8_t* a_buf = attr_buffer + CONSADDR (cursor_row) + cursor_col ;
    uint8_t fg = show ? color_bg : color_fg ;
    uint8_t bg = show ? color_fg : color_bg ;
    *a_buf = CONS_ATTR(fg, bg) ;
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
    cursor_eol = false ;
    
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
    cursor_eol = false ;

    cons_show_cursor(true) ;
}

static void move_cursor_limited(int row, int col) {
    if (cursor_row >= scroll_region_start && cursor_row <= scroll_region_end) {
        move_cursor_within_region(row, col, scroll_region_start, scroll_region_end) ;
    }
}

static void init_cursor(int row, int col) {
    cursor_row = -1;
    cursor_col = -1;
    move_cursor_within_region(row, col, 0, CONS_LAST_ROW) ;
}

static const uint8_t graphics_char_mapping[31] = {
    0x04, // diamond/caret
    0xB1, // scatter
    0x0B, // HT
    0x0C, // FF
    0x0D, // CR
    0x0E, // LF
    0xF8, // degree symbol
    0xF1, // plusminus
    0x0F, // NL
    0x10, // VT
    0xD9, // left-top corner
    0xBF, // left-bottom corner
    0xDA, // right-bottom corner
    0xC0, // right-top corner
    0xC5, // cross
    0x11, // horizontal line 1
    0x12, // horizontal line 2
    0xC4, // horizontal line 3
    0x13, // horizontal line 4
    0x5F, // horizontal line 5
    0xC3, // right "T"
    0xB4, // left "T"
    0xC1, // top "T"
    0xC2, // bottom "T"
    0xB3, // vertical line
    0xF3, // less-equal
    0xF2, // greater-equal
    0xE3, // pi
    0x1C, // not equal
    0x9C, // pound sterling
    0xFA  // center dot
} ;

static uint8_t map_graphics(uint8_t c) {
    if (c < 0x60 || c > 0x7E) {
        return c ;
    }

    return graphics_char_mapping[c - 0x60] ;
}

static void cons_put_char(char c) {
    if (cursor_eol) {
        move_cursor_wrap(cursor_row + 1, 0) ;
        cursor_eol = false ;
    }

    int addr = CONSADDR (cursor_row) + cursor_col ;
    char_buffer[addr] = *charset ? map_graphics(c) : c ;
    uint8_t fg = (attr & ATTR_INVERSE) ? color_bg : color_fg ;
    uint8_t bg = (attr & ATTR_INVERSE) ? color_fg : color_bg ;

    if (attr & ATTR_BOLD) {
        fg += 8 ;
    }

    if (attr & ATTR_UNDERLINE) {
        bg = 5 ;
    }

    attr_buffer[addr] = CONS_ATTR (fg, bg) ;
    
    if (cursor_col == TEXTMODE_COLS - 1) {
        // cursor stays in last column but will wrap if another character is typed
        cons_show_cursor(cursor_shown);
        cursor_eol = true ;
    } else {
        init_cursor(cursor_row, cursor_col + 1) ;
    }
}

static void cons_fill_region(uint8_t xs, uint8_t ys, uint8_t xe, uint8_t ye, char c, uint8_t fg, uint8_t bg) {
    for (int addr = CONSADDR(ys) + xs; addr <= CONSADDR(ye) + xe; addr++) {
        char_buffer[addr] = c;
        attr_buffer[addr] =  CONS_ATTR (fg, bg) ;
    }
}

static void cons_insert(uint8_t x, uint8_t y, uint8_t n, uint8_t fg, uint8_t bg) {
    if (x >= TEXTMODE_COLS || y >= CONS_LAST_ROW) {
        return ;
    }

    int addr = CONSADDR (y) + x  + n ;

    for (int i = 0; i < n; i++) {
        char_buffer[addr] = char_buffer[addr - n] ;
        attr_buffer[addr] = attr_buffer[addr - n] ;
        addr-- ;
    }

    addr = CONSADDR (y) + x ;

    for (int i = 0; i < n; i++) {
        char_buffer[addr] = ' ' ;
        attr_buffer[addr] = CONS_ATTR(fg, bg) ;
        addr++ ;
    }
}

static void cons_delete(uint8_t x, uint8_t y, uint8_t n, uint8_t fg, uint8_t bg) {
    if (x >= TEXTMODE_COLS || y >= CONS_LAST_ROW) {
        return ;
    }

    int addr = CONSADDR (y) + x ;

    for (int i = 0; i < n; i++) {
        char_buffer[addr] = char_buffer[addr + n] ;
        attr_buffer[addr] = attr_buffer[addr + n] ;
        addr++ ;
    }

    addr = CONSADDR (y) + x + n;

    for (int i = 0; i < n; i++) {
        char_buffer[addr] = ' ' ;
        attr_buffer[addr] = CONS_ATTR(fg, bg) ;
        addr-- ;
    }
}

static void cons_process_text(char c) {
    switch (c) {
        case 5: // ENQ => send answer-back string
        cons_send_string("murm-pdp-11") ;
        break;
      
        case 7: // BEL => produce beep
        //   sound_play_tone(config_get_audible_bell_frequency(), 
        //                   config_get_audible_bell_duration(), 
        //                   config_get_audible_bell_volume(), 
        //                   false);
        //   framebuf_flash_screen(config_get_visual_bell_color(), config_get_visual_bell_duration());
            // graphics_set_flashmode(false, true) ;
            break ;
      
        case 0x08:   // backspace
        case 0x7F: { // delete
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
    case 0x0B:   // vertical tab (interpreted as newline)
        move_cursor_wrap(cursor_row + 1, cursor_col) ;
        break ;
    case 0x0C:   // form feed (interpreted as newline)
    case '\r': // carriage return
        move_cursor_wrap(cursor_row, 0) ;
        break ;

    case 0x0E:  // SO
        charset = &charset_G1; 
        break;

    case 0x0F:  // SI
        charset = &charset_G0; 
        break;

    default: // regular character
        if (c >= 32 ) {
            cons_put_char(c) ;
        }
        break;
    }
}

void cons_reset() {
    saved_col = 0;
    saved_row = 0;
    cursor_shown = true;
    color_fg = COLOR_FG;
    color_bg = 0;
    scroll_region_start = 0;
    scroll_region_end = CONS_LAST_ROW ;
    attr = 0;
    cursor_eol = false ;
    saved_attr = 0;
    charset_G0 = CS_TEXT;
    charset_G1 = CS_GRAPHICS;
    saved_charset_G0 = CS_TEXT;
    saved_charset_G1 = CS_GRAPHICS;
    charset = &charset_G0;
}

static void cons_process_command(char start_char, char final_char, uint8_t num_params, uint8_t *params) {
  // NOTE: num_params>=1 always holds, if no parameters were received then params[0]=0
    if (final_char == 'l' || final_char == 'h') {
        bool enabled = final_char == 'h';
        if (start_char == '?') {
            switch(params[0]) {
                case 2:
                    if (!enabled) {
                        cons_reset();
                    }
                    break ;

                case 3: // switch 80/132 columm mode - 132 columns not supported but we can clear the screen
                    cons_cls() ;
                    break ;

                case 4: // enable smooth scrolling (emulated via scroll delay)
                    //framebuf_set_scroll_delay(enabled ? config_get_terminal_scrolldelay() : 0);
                    break;
              
                case 5: // invert screen
                    // framebuf_set_screen_inverted(enabled);
                    break;
          
                case 6: // origin mode
                    // origin_mode = enabled; 
                    move_cursor_limited(scroll_region_start, 0); 
                    break;
              
                case 7: // auto-wrap mode
                    // auto_wrap_mode = enabled; 
                    break;

                case 12: // local echo (send-receive mode)
                    // localecho = !enabled;
                    break;
              
                case 25: // show/hide cursor
                    cursor_shown = enabled;
                    cons_show_cursor(cursor_shown);
                    break;
            }
        } else if (start_char == 0) {
            switch (params[0]) {
                case 4: // insert mode
                    // insert_mode = enabled;
                    break;
            }
        }
    } else if (final_char == 'J') {
        switch (params[0]) {
            case 0:
                // for (int i = cursor_row; i < 27; i++) framebuf_set_row_attr(i, 0);
                cons_fill_region(cursor_col, cursor_row, TEXTMODE_COLS - 1, CONS_LAST_ROW, ' ', color_fg, color_bg);
                break;
          
            case 1:
                // for(int i=0; i<cursor_row; i++) framebuf_set_row_attr(i, 0);
                cons_fill_region(0, 0, cursor_col, cursor_row, ' ', color_fg, color_bg);
                break;
          
            case 2:
                // for(int i=0; i<framebuf_get_nrows(); i++) framebuf_set_row_attr(i, 0);
                cons_fill_region(0, 0, TEXTMODE_COLS - 1, CONS_LAST_ROW, ' ', color_fg, color_bg);
                break;
        }

        // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
        cons_show_cursor(cursor_shown);
    } else if (final_char == 'K') {
        switch (params[0]) {
            case 0:
                cons_fill_region(cursor_col, cursor_row, TEXTMODE_COLS - 1, CONS_LAST_ROW, ' ', color_fg, color_bg);
                break;
          
            case 1:
                cons_fill_region(0, cursor_row, cursor_col, cursor_row, ' ', color_fg, color_bg);
                break;
          
            case 2:
                cons_fill_region(0, cursor_row, TEXTMODE_COLS - 1, cursor_row, ' ', color_fg, color_bg);
                break;
        }

        // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
        cons_show_cursor(cursor_shown);
    } else if (final_char == 'A') {
        move_cursor_limited(cursor_row - MAX(1, params[0]), cursor_col);
    } else if (final_char == 'B') {
        move_cursor_limited(cursor_row + MAX(1, params[0]), cursor_col);
    } else if (final_char == 'C' || final_char == 'a') {
        move_cursor_limited(cursor_row, cursor_col + MAX(1, params[0]));
    } else if (final_char == 'D' || final_char == 'j') {
        move_cursor_limited(cursor_row, cursor_col - MAX(1, params[0]));
    } else if (final_char == 'E' || final_char == 'e') {
        move_cursor_limited(cursor_row + MAX(1, params[0]), 0);
    } else if (final_char == 'F' || final_char == 'k') {
        move_cursor_limited(cursor_row - MAX(1, params[0]), 0);
    } else if (final_char == 'd') {
        move_cursor_limited(MAX(1, params[0]), cursor_col);
    } else if (final_char == 'G' || final_char == '`') {
        move_cursor_limited(cursor_row, MAX(1, params[0])-1);
    } else if (final_char == 'H' || final_char=='f') {
        int top_limit    = scroll_region_start ;
        int bottom_limit = scroll_region_end ;
        move_cursor_within_region(top_limit + MAX(params[0],1) - 1, num_params < 2 ? 0 : MAX(params[1],1) - 1, top_limit, bottom_limit);
    } else if (final_char == 'I') {
        int n = MAX(1, params[0]);
        int col = cursor_col + 1;
        while (n > 0 && col < TEXTMODE_COLS - 1) {
                while (col < TEXTMODE_COLS - 1) {
                    col++;
                }
                n--;
        }
        move_cursor_limited(cursor_row, col); 
    } else if (final_char == 'Z') {
        int n = MAX(1, params[0]);
        int col = cursor_col-1;
        while (n > 0 && col > 0) {
            while (col > 0) {
                col--;
            }
            n--;
        }
        move_cursor_limited(cursor_row, col); 
    } else if (final_char == 'L' || final_char == 'M') {
        int n = MAX(1, params[0]);
        int bottom_limit = scroll_region_end ;
        cons_show_cursor(false);
        cons_scroll_region(cursor_row, bottom_limit, final_char == 'M' ? n : -n, color_fg, color_bg);
        // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
        cons_show_cursor(cursor_shown);
    } else if (final_char=='@') {
        int n = MAX(1, params[0]);
        cons_show_cursor(false);
        cons_insert(cursor_col, cursor_row, n, color_fg, color_bg);
        // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
        cons_show_cursor(cursor_shown);
    } else if (final_char == 'P') {
        int n = MAX(1, params[0]);
        cons_delete(cursor_col, cursor_row, n, color_fg, color_bg);
        // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
        cons_show_cursor(cursor_shown);
    } else if (final_char == 'S' || final_char == 'T') {
        int top_limit    = scroll_region_start;
        int bottom_limit = scroll_region_end;
        int n = MAX(1, params[0]);
        cons_show_cursor(false);
        while (n--) {
            cons_scroll_region(top_limit, bottom_limit, final_char == 'S' ? n : -n, color_fg, color_bg);
        }
        // cur_attr = framebuf_get_attr(cursor_col, cursor_row);
        cons_show_cursor(cursor_shown);
    } else if (final_char == 'g') {
        int p = params[0];
        if (p==0) {
            // tabs[cursor_col] = false;
        } else if (p == 3) {
            // memset(tabs, 0, TEXTMODE_COLS) ;
        }
    } else if (final_char == 'm') {
        unsigned int i;
        for (i = 0; i < num_params; i++) {
            int p = params[i] ;
            if (p == 0) {
                color_fg = COLOR_FG;
                color_bg = 0;
                attr     = 0 ;
                cursor_shown = true;
                cons_show_cursor(cursor_shown);
            } else if (p == 1) {
                attr |= ATTR_BOLD;
            } else if (p == 4) {
                attr |= ATTR_UNDERLINE;
            } else if (p == 5) {
                attr |= ATTR_BLINK;
            } else if (p == 7) {
                attr |= ATTR_INVERSE;
            } else if (p == 22) {
                attr &= ~ATTR_BOLD;
            } else if( p==24 ) {
                attr &= ~ATTR_UNDERLINE;
            } else if(p == 25) {
                attr &= ~ATTR_BLINK;
            } else if(p == 27) {
                attr &= ~ATTR_INVERSE;
            } else if (p >= 30 && p <= 37) {
                color_fg = p - 30;
            } else if (p == 38 && num_params >= i + 2 && params[i+1] == 5) {
                color_fg = params[i + 2] & 0x0F;
                i += 2;
            } else if (p == 39) {
                color_fg = COLOR_FG;
            } else if (p >= 40 && p <= 47) {
                color_bg = p - 40 ;
            } else if (p == 48 && num_params >= i + 2 && params[i + 1] == 5) {
                color_bg = params[i+2] & 0x0F;
                i+=2;
            } else if (p == 49) {
                color_bg = 0;
            }

            cons_show_cursor(cursor_shown);
        }
    } else if (final_char == 'r') {
        if (num_params == 2 && params[1] > params[0]) {
            scroll_region_start = MAX(params[0], 1)-1;
            scroll_region_end   = MIN(params[1], CONS_LAST_ROW) ;
        } else if (params[0] == 0) {
            scroll_region_start = 0;
            scroll_region_end   = CONS_LAST_ROW;
        }

        move_cursor_within_region(scroll_region_start, 0, scroll_region_start, scroll_region_end);
    } else if (final_char == 's') {
        saved_row = cursor_row;
        saved_col = cursor_col;
        saved_eol = cursor_eol;
        //   saved_origin_mode = origin_mode;
          saved_fg  = color_fg;
          saved_bg  = color_bg;
          saved_attr = attr;
          saved_charset_G0 = charset_G0;
          saved_charset_G1 = charset_G1;
    } else if (final_char == 'u') {
        move_cursor_limited(saved_row, saved_col);
        // origin_mode = saved_origin_mode;      
        cursor_eol = saved_eol;
        color_fg = saved_fg;
        color_bg = saved_bg;
        attr = saved_attr;
        charset_G0 = saved_charset_G0;
        charset_G1 = saved_charset_G1;
    } else if (final_char == 'c') {
        cons_send_string("\033[?6c");
    } else if (final_char == 'n') {
        if (params[0] == 5) {
          // terminal status report
          cons_send_string("\033[0n");
        } else if (params[0] == 6) {
          // cursor position report
            int top_limit = scroll_region_start ;
            char buf[20];
            snprintf(buf, 20, "\033[%u;%uR", cursor_row-top_limit+1, cursor_col+1);
            cons_send_string(buf);
        }
    }
}

static uint8_t get_charset(char c) {
    switch (c) {
        case 'A' : return CS_TEXT;
        case 'B' : return CS_TEXT;
        case '0' : return CS_GRAPHICS;
        case '1' : return CS_TEXT;
        case '2' : return CS_GRAPHICS;
    }

    return CS_TEXT;
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
            if (c == 0x1B) {
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
            
                case 0x1B: cons_put_char(c); break;                           // escaped ESC
                case 'c': cons_reset(); break;                           // reset
                case '7': cons_process_command(0, 's', 0, NULL); break;  // save cursor position
                case '8': cons_process_command(0, 'u', 0, NULL); break;  // restore cursor position
                case 'H':                                                // set tab
                    // tabs[cursor_col] = true;
                    break;
                case 'J': cons_process_command(0, 'J', 0, NULL); break;  // clear to end of screen
                case 'K': cons_process_command(0, 'K', 0, NULL); break;  // clear to end of row
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
            cons_process_command(start_char, c, num_params, params);
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
                cons_fill_region(0, top_limit, TEXTMODE_COLS - 1, bottom_limit, 'E', color_fg, color_bg);
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
            charset_G0 = get_charset(c);
        } else if (start_char==')') {
            charset_G1 = get_charset(c);
        }

        terminal_state = TS_NORMAL;
        break;
      }
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
        cons_put_char_vt100(*p++) ;
    }
}
