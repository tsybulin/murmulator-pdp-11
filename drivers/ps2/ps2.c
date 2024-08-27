#include "ps2.h"
#include <pico/stdlib.h>
#include <stdbool.h>
#include "string.h"
#include "hardware/irq.h"

#include "graphics.h"

volatile int bitcount;
static uint8_t ps2bufsize = 0;
uint8_t ps2buffer[KBD_BUFFER_SIZE];
uint8_t kbloop = 0;

uint8_t led_status = 0b000;

#define PS2_ERR_NONE    0

static uint8_t ignoreBytes = 0 ;
static bool breakcode = false, extkey = false ;
static uint8_t keyboard_modifiers  = 0 ;
ps2_handler_t ps2_handler ;

static const uint8_t __in_flash(".keymaps") scancodes[136] =  {
    HID_KEY_NONE,      HID_KEY_F9,             HID_KEY_NONE,       HID_KEY_F5,              HID_KEY_F3,              HID_KEY_F1,        HID_KEY_F2,          HID_KEY_F12,      // 0x00-0x07
    HID_KEY_NONE,      HID_KEY_F10,            HID_KEY_F8,         HID_KEY_F6,              HID_KEY_F4,              HID_KEY_TAB,       HID_KEY_GRAVE,       HID_KEY_NONE,     // 0x08-0x0F
    HID_KEY_NONE,      HID_KEY_ALT_LEFT,       HID_KEY_SHIFT_LEFT, HID_KEY_NONE,            HID_KEY_CONTROL_LEFT,    HID_KEY_Q,         HID_KEY_1,           HID_KEY_NONE,     // 0x10-0x17
    HID_KEY_NONE,      HID_KEY_NONE,           HID_KEY_Z,          HID_KEY_S,               HID_KEY_A,               HID_KEY_W,         HID_KEY_2,           HID_KEY_NONE,     // 0x18-0x1F
    HID_KEY_NONE,      HID_KEY_C,              HID_KEY_X,          HID_KEY_D,               HID_KEY_E,               HID_KEY_4,         HID_KEY_3,           HID_KEY_NONE,     // 0x20-0x27
    HID_KEY_NONE,      HID_KEY_SPACE,          HID_KEY_V,          HID_KEY_F,               HID_KEY_T,               HID_KEY_R,         HID_KEY_5,           HID_KEY_NONE,     // 0x28-0x2F
    HID_KEY_NONE,      HID_KEY_N,              HID_KEY_B,          HID_KEY_H,               HID_KEY_G,               HID_KEY_Y,         HID_KEY_6,           HID_KEY_NONE,     // 0x30-0x37
    HID_KEY_NONE,      HID_KEY_NONE,           HID_KEY_M,          HID_KEY_J,               HID_KEY_U,               HID_KEY_7,         HID_KEY_8,           HID_KEY_NONE,     // 0x38-0x3F
    HID_KEY_NONE,      HID_KEY_COMMA,          HID_KEY_K,          HID_KEY_I,               HID_KEY_O,               HID_KEY_0,         HID_KEY_9,           HID_KEY_NONE,     // 0x40-0x47
    HID_KEY_NONE,      HID_KEY_PERIOD,         HID_KEY_SLASH,      HID_KEY_L,               HID_KEY_SEMICOLON,       HID_KEY_P,         HID_KEY_MINUS,       HID_KEY_NONE,     // 0x48-0x4F
    HID_KEY_NONE,      HID_KEY_NONE,           HID_KEY_APOSTROPHE, HID_KEY_NONE,            HID_KEY_BRACKET_LEFT,    HID_KEY_EQUAL,     HID_KEY_NONE,        HID_KEY_NONE,     // 0x50-0x57
    HID_KEY_CAPS_LOCK, HID_KEY_SHIFT_RIGHT,    HID_KEY_ENTER,      HID_KEY_BRACKET_RIGHT,   HID_KEY_NONE,            HID_KEY_BACKSLASH, HID_KEY_NONE,        HID_KEY_NONE,     // 0x58-0x5F
    HID_KEY_NONE,      HID_KEY_EUROPE_2,       HID_KEY_NONE,       HID_KEY_NONE,            HID_KEY_NONE,            HID_KEY_NONE,      HID_KEY_BACKSPACE,   HID_KEY_NONE,     // 0x60-0x67
    HID_KEY_NONE,      HID_KEY_KEYPAD_1,       HID_KEY_NONE,       HID_KEY_KEYPAD_4,        HID_KEY_KEYPAD_7,        HID_KEY_NONE,      HID_KEY_NONE,        HID_KEY_NONE,     // 0x68-0x6F
    HID_KEY_KEYPAD_0,  HID_KEY_KEYPAD_DECIMAL, HID_KEY_KEYPAD_2,   HID_KEY_KEYPAD_5,        HID_KEY_KEYPAD_6,        HID_KEY_KEYPAD_8,  HID_KEY_ESCAPE,      HID_KEY_NUM_LOCK, // 0x70-0x77
    HID_KEY_F11,       HID_KEY_KEYPAD_ADD,     HID_KEY_KEYPAD_3,   HID_KEY_KEYPAD_SUBTRACT, HID_KEY_KEYPAD_MULTIPLY, HID_KEY_KEYPAD_9,  HID_KEY_SCROLL_LOCK, HID_KEY_NONE,     // 0x78-0x7F
    HID_KEY_NONE,      HID_KEY_NONE,           HID_KEY_NONE,       HID_KEY_F7,              HID_KEY_NONE,            HID_KEY_NONE,      HID_KEY_NONE,        HID_KEY_NONE      // 0x80-0x87
} ;

volatile int16_t ps2_error = PS2_ERR_NONE;

void ps2poll();
void keyboard_process_byte(uint8_t b) ;

static void clock_lo(void) {
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_OUT);
    gpio_put(KBD_CLOCK_PIN, 0);
}

static inline void clock_hi(void) {
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_OUT);
    gpio_put(KBD_CLOCK_PIN, 1);
}

static bool clock_in(void) {
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);
    asm("nop");
    return gpio_get(KBD_CLOCK_PIN);
}

static void data_lo(void) {
    gpio_set_dir(KBD_DATA_PIN, GPIO_OUT);
    gpio_put(KBD_DATA_PIN, 0);
}

static void data_hi(void) {
    gpio_set_dir(KBD_DATA_PIN, GPIO_OUT);
    gpio_put(KBD_DATA_PIN, 1);
}

static inline bool data_in(void) {
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);
    asm("nop");
    return gpio_get(KBD_DATA_PIN);
}

static void inhibit(void) {
    clock_lo();
    data_hi();
}

static void idle(void) {
    clock_hi();
    data_hi();
}

#define wait_us(us)     busy_wait_us_32(us)
#define wait_ms(ms)     busy_wait_ms(ms)

static inline uint16_t wait_clock_lo(uint16_t us) {
    while (clock_in() && us) {
        asm("");
        wait_us(1);
        us--;
    }
    return us;
}

static inline uint16_t wait_clock_hi(uint16_t us) {
    while (!clock_in() && us) {
        asm("");
        wait_us(1);
        us--;
    }
    return us;
}

static inline uint16_t wait_data_lo(uint16_t us) {
    while (data_in() && us) {
        asm("");
        wait_us(1);
        us--;
    }
    return us;
}

static inline uint16_t wait_data_hi(uint16_t us) {
    while (!data_in() && us) {
        asm("");
        wait_us(1);
        us--;
    }
    return us;
}

#define WAIT(stat, us, err) do { \
    if (!wait_##stat(us)) { \
        ps2_error = err; \
        goto ERROR; \
    } \
} while (0)

static void int_on(void) {
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);
    gpio_set_irq_enabled(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true);
}

static void int_off(void) {
    gpio_set_irq_enabled(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, false);
}

static int16_t ps2_recv_response(void) {
    uint8_t retry = 25;
    int16_t c = -1;
    while (retry-- && (c = ps2buffer[ps2bufsize]) == -1) {
        wait_ms(1);
    }
    return c;
}

int16_t keyboard_send(uint8_t data) {
    bool parity = true;
    ps2_error = PS2_ERR_NONE;
    int_off();
    inhibit();
    wait_us(200);
    data_lo();
    wait_us(200);
    clock_hi();
    WAIT(clock_lo, 15000, 1) ;

    for (uint8_t i = 0; i < 8; i++) {
        wait_us(15);
        if (data & (1 << i)) {
            parity = !parity;
            data_hi();
        }
        else {
            data_lo();
        }
        WAIT(clock_hi, 100, (int16_t) (2 + i * 0x10));
        WAIT(clock_lo, 100, (int16_t) (3 + i * 0x10));
    }

    /* Parity bit */
    wait_us(15);
    if (parity) { data_hi(); }
    else { data_lo(); }
    WAIT(clock_hi, 100, 4);
    WAIT(clock_lo, 100, 5);

    /* Stop bit */
    wait_us(15);
    data_hi();

    /* Ack */
    WAIT(data_lo, 100, 6); // check Ack
    WAIT(data_hi, 100, 7);
    WAIT(clock_hi, 100, 8);

    memset(ps2buffer, 0x00, sizeof ps2buffer);
    //ringbuf_reset(&rbuf);   // clear buffer
    idle();
    int_on();
    return ps2_recv_response();
ERROR:
    gprintf("KBD error %02X", ps2_error);
    ps2_error = 0;
    idle();
    int_on();
    return -0xf;
}

void keyboard_toggle_led(uint8_t led) {
    led_status ^= led;

    keyboard_send(0xED);
    busy_wait_ms(50);
    keyboard_send(led_status);
}

void KeyboardHandler(void) {
    static uint8_t incoming = 0;
    static uint32_t prev_ms = 0;
    uint32_t now_ms;
    uint8_t n, val;

    val = gpio_get(KBD_DATA_PIN);
    now_ms = time_us_64();
    if (now_ms - prev_ms > 250) {
        bitcount = 0;
        incoming = 0;
    }
    prev_ms = now_ms;
    n = bitcount - 1;
    if (n <= 7) {
        incoming |= (val << n);
    }
    bitcount++;
    if (bitcount == 11) {
        if (ps2bufsize < KBD_BUFFER_SIZE) {
            ps2buffer[ps2bufsize++] = incoming;
            ps2poll();
        }
        bitcount = 0;
        incoming = 0;
    }
    kbloop = 1;
}

void keyboard_init(ps2_handler_t handler) {
    bitcount = 0;
    breakcode = false ;
    extkey = false ;
    ignoreBytes = 0 ;
    keyboard_modifiers = 0 ;
    ps2_handler = handler ;
    memset(ps2buffer, 0, KBD_BUFFER_SIZE);

    gpio_init(KBD_CLOCK_PIN);
    gpio_init(KBD_DATA_PIN);
    gpio_disable_pulls(KBD_CLOCK_PIN);
    gpio_disable_pulls(KBD_DATA_PIN);
    gpio_set_drive_strength(KBD_CLOCK_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(KBD_DATA_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true, (gpio_irq_callback_t)&KeyboardHandler) ;

    return;
}

void ps2poll() {
    uint32_t i, len;
    if (!ps2bufsize) {
        return ;
    }

    switch (ps2buffer[0]) {
        case 0xF0:
        case 0xE0:
        case 0xE1:
            len = 2;
            break;
        default:
            len = 1;
            break;
    }

    if (ps2bufsize < len) {
        return ;
    }

    if (ps2buffer[0] == 0xE0) {
        if (ps2buffer[1] == 0xF0) {
            len = 3 ;
        }
    }

    if (ps2bufsize < len) {
        return ;
    }

    if (len == 1) {
        keyboard_process_byte(ps2buffer[0]) ;
    }
    if (len == 2) {
        keyboard_process_byte(ps2buffer[0]) ;
        keyboard_process_byte(ps2buffer[1]) ;
    }
    if (len == 3) {
        keyboard_process_byte(ps2buffer[0]) ;
        keyboard_process_byte(ps2buffer[1]) ;
        keyboard_process_byte(ps2buffer[2]) ;
    }

    for (i = len; i < KBD_BUFFER_SIZE; i++) {
        ps2buffer[i - len] = ps2buffer[i];
    }

    ps2bufsize -= len;
}

void cons_printf(const char *__restrict format, ...) ;

void keyboard_key_change(uint8_t key, bool make) {
    // cons_printf("keyboard_key_change key:0x%02X make:%d\r\n", key, make) ;
    uint8_t mod = 0 ;
    switch (key) {
        case HID_KEY_SHIFT_LEFT:    mod = KEYBOARD_MODIFIER_LEFTSHIFT;  break;
        case HID_KEY_SHIFT_RIGHT:   mod = KEYBOARD_MODIFIER_RIGHTSHIFT; break;
        case HID_KEY_CONTROL_LEFT:  mod = KEYBOARD_MODIFIER_LEFTCTRL;   break;
        case HID_KEY_CONTROL_RIGHT: mod = KEYBOARD_MODIFIER_RIGHTCTRL;  break;
        case HID_KEY_ALT_LEFT:      mod = KEYBOARD_MODIFIER_LEFTALT;    break;
        case HID_KEY_ALT_RIGHT:     mod = KEYBOARD_MODIFIER_RIGHTALT;   break;
        case HID_KEY_GUI_LEFT:      mod = KEYBOARD_MODIFIER_LEFTGUI;    break;
        case HID_KEY_GUI_RIGHT:     mod = KEYBOARD_MODIFIER_RIGHTGUI;   break;

        case HID_KEY_NUM_LOCK:      if (make) keyboard_toggle_led(PS2_LED_NUM_LOCK); break;
        case HID_KEY_SCROLL_LOCK:   if (make) keyboard_toggle_led(PS2_LED_SCROLL_LOCK); break;
        case HID_KEY_CAPS_LOCK:     if (make) keyboard_toggle_led(PS2_LED_CAPS_LOCK); break;
    }
    if (mod != 0) {
        if (make) {
            keyboard_modifiers |= mod ;
        } else {
            keyboard_modifiers &= ~mod ;
        }
    } else if (make && ps2_handler) {
        ps2_handler(key, keyboard_modifiers) ;
    }
}

// translate scancode to keycode
void keyboard_process_byte(uint8_t b) {
    // cons_printf("keyboard_process_byte b:0x%02X\r\n", b) ;
    uint8_t key = HID_KEY_NONE ;

    if (ignoreBytes > 0) {
        ignoreBytes-- ;
        return ;
    } else if (b == 0xE0) {
        extkey = true ;
    } else if (b == 0xE1) {
        // pause/break key sends the following sequence when pressed
        // 0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xF0, 0x77
        // and nothing when released.
        // Since no other key sends 0xE1, we just take 0xE1 as meaning
        // "pause/break has been pressed and released" and ignore the
        // other bytes
        
        ignoreBytes = 7 ;
        keyboard_key_change(HID_KEY_PAUSE, true) ;
        keyboard_key_change(HID_KEY_PAUSE, false) ;
    } else if (extkey) {
        switch(b) {
            case 0x11: key = HID_KEY_ALT_RIGHT;     break;
            case 0x14: key = HID_KEY_CONTROL_RIGHT; break;
            case 0x1F: key = HID_KEY_GUI_LEFT;      break;
            case 0x27: key = HID_KEY_GUI_RIGHT;     break;
            case 0x4A: key = HID_KEY_KEYPAD_DIVIDE; break;
            case 0x5A: key = HID_KEY_RETURN;        break;
            case 0x69: key = HID_KEY_END;           break;
            case 0x6B: key = HID_KEY_ARROW_LEFT;    break;
            case 0x6C: key = HID_KEY_HOME;          break;
            case 0x70: key = HID_KEY_INSERT;        break;
            case 0x71: key = HID_KEY_DELETE;        break;
            case 0x72: key = HID_KEY_ARROW_DOWN;    break;
            case 0x74: key = HID_KEY_ARROW_RIGHT;   break;
            case 0x75: key = HID_KEY_ARROW_UP;      break;
            case 0x7A: key = HID_KEY_PAGE_DOWN;     break;
            case 0x7C: key = HID_KEY_PRINT_SCREEN;  break;
            case 0x7D: key = HID_KEY_PAGE_UP;       break;
            case 0x7E: key = HID_KEY_PAUSE;         break;
        }
    } else if (b < 136) {
        key = scancodes[b] ;
    }

    if (key != HID_KEY_NONE) {
        keyboard_key_change(key, !breakcode) ;
    }

    if (extkey && b != 0xE0 && b != 0xF0) {
        extkey = false;
    } 
    breakcode = (b == 0xF0) ;
}
