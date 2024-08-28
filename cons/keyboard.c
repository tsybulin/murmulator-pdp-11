#include "cons.h"

#include "ps2.h"
#include "hid.h"
#include "graphics.h"

#include <pico/platform.h>

// -------------------------------------------  keyboard layouts (languages)  -------------------------------------------

struct IntlMapStruct {
    uint8_t mapNormal[71] ;
    uint8_t mapShift[71] ;
    struct { int code; bool shift; int character; } mapOther[10] ;
    struct { int code; int character; } mapAltGr[12] ;
    uint8_t keypadDecimal ;
} ;

const struct IntlMapStruct __in_flash(".keymaps") intlMap = {
    { // normal
        0    ,0    ,0    ,0    ,'a'  ,'b'  ,'c'  ,'d'  ,
        'e'  ,'f'  ,'g'  ,'h'  ,'i'  ,'j'  ,'k'  ,'l'  ,
        'm'  ,'n'  ,'o'  ,'p'  ,'q'  ,'r'  ,'s'  ,'t'  ,
        'u'  ,'v'  ,'w'  ,'x'  ,'y'  ,'z'  ,'1'  ,'2'  ,
        '3'  ,'4'  ,'5'  ,'6'  ,'7'  ,'8'  ,'9'  ,'0'  ,
        KEY_ENTER, KEY_ESC, KEY_BACKSPACE, KEY_TAB, ' ', '-', '=', '[',
        ']'  ,'\\' ,'\\' ,';'  ,'\'' ,'`'  ,','  ,'.'  ,
        '/'  ,0 , KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6   ,
        KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_PRSCRN
     },
    { // shift
        0    ,0    ,0    ,0    ,'A'  ,'B'  ,'C'  ,'D'  ,
        'E'  ,'F'  ,'G'  ,'H'  ,'I'  ,'J'  ,'K'  ,'L'  ,
        'M'  ,'N'  ,'O'  ,'P'  ,'Q'  ,'R'  ,'S'  ,'T'  ,
        'U'  ,'V'  ,'W'  ,'X'  ,'Y'  ,'Z'  ,'!'  ,'@'  ,
        '#'  ,'$'  ,'%'  ,'^'  ,'&'  ,'*'  ,'('  ,')'  ,
        KEY_ENTER ,KEY_ESC   ,KEY_BACKSPACE  ,KEY_TAB ,' '  ,'_'  ,'+'  ,'{'  ,
        '}'  ,'|'  ,'|'  ,':'  ,'"'  ,'~'  ,'<'  ,'>'  ,
        '?'  ,0 , KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6   ,
        KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_PRSCRN
    },
    {
        {0x64, 0, '\\'},
        {0x64, 1, '|'},
        {-1,-1}
    },
    {
        {-1,-1}
    },
    '.'
} ;

static const int keyMapSpecial[] = {KEY_PAUSE, KEY_INSERT, KEY_HOME, KEY_PUP, KEY_DELETE, KEY_END, KEY_PDOWN, KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP} ;
static const int keyMapKeypad[]    = {'\\', '*', '-', '+', KEY_ENTER, KEY_END, KEY_DOWN, KEY_PDOWN, KEY_LEFT, 0, KEY_RIGHT, KEY_HOME, KEY_UP, KEY_PUP, KEY_INSERT, KEY_DELETE} ;
static const int keyMapKeypadNum[] = {'\\', '*', '-', '+', KEY_ENTER , '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.'} ;

static uint8_t keyboard_led_status = 0 ;

uint8_t keyboard_map_key_ascii(uint8_t key, uint8_t modifier, bool *isaltcode) {
    uint8_t i, ascii = 0 ;
    static uint8_t altcodecount = 0;
    static uint8_t altcode = 0;
    
    if (isaltcode != NULL) {
        *isaltcode = false ;
    }

    if ((modifier & KEYBOARD_MODIFIER_LEFTALT) !=0 && key >= HID_KEY_KEYPAD_1 && key <= HID_KEY_KEYPAD_0) {
        altcode *= 10 ;
        altcode += (key == HID_KEY_KEYPAD_0) ? 0 : (key - HID_KEY_KEYPAD_1 + 1) ;
        altcodecount++ ;
        if (altcodecount == 3) {
            if (isaltcode != NULL) {
                *isaltcode = true ;
            }
            ascii = altcode ;
            altcode = 0 ;
            altcodecount = 0 ;
        }

        return ascii;
    } else {
        altcode = 0 ;
        altcodecount = 0 ;
    }

    if (!modifier) {
        if (key == HID_KEY_CAPS_LOCK) {
            keyboard_led_status ^= KEYBOARD_LED_CAPSLOCK ;
        } else if (key == HID_KEY_NUM_LOCK) {
            keyboard_led_status ^= KEYBOARD_LED_NUMLOCK ;
        } else if (key == HID_KEY_SCROLL_LOCK) {
            keyboard_led_status ^= KEYBOARD_LED_SCROLLLOCK ;
        }
    }

    if (modifier & KEYBOARD_MODIFIER_RIGHTALT) {
        for (i = 0; intlMap.mapAltGr[i].code >= 0; i++) {
            if (intlMap.mapAltGr[i].code == key) {
                ascii = intlMap.mapAltGr[i].character ;
                break ;
            }
        }
    } else if (key <= HID_KEY_PRINT_SCREEN) {
        bool ctrl  = (modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL)) !=0 ;
        bool shift = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0 ;
        bool caps  = (key >= HID_KEY_A) && (key <= HID_KEY_Z) && (keyboard_led_status & KEYBOARD_LED_CAPSLOCK) != 0 ;

        if(shift ^ caps) {
            ascii = intlMap.mapShift[key] ;
        } else {
            ascii = intlMap.mapNormal[key] ;
        }

        if (ctrl && ascii >= 0x40 && ascii < 0x7f) {
            ascii &= 0x1f ;
        }
    } else if ((key >= HID_KEY_PAUSE) && (key <= HID_KEY_ARROW_UP)) {
        ascii = keyMapSpecial[key - HID_KEY_PAUSE] ;
    } else if (key == HID_KEY_RETURN) {
        ascii = HID_KEY_RETURN ;
    } else if ((key >= HID_KEY_KEYPAD_DIVIDE) && (key <= HID_KEY_KEYPAD_DECIMAL)) {
        if ((keyboard_led_status & KEYBOARD_LED_NUMLOCK) == 0) {
            ascii = keyMapKeypad[key - HID_KEY_KEYPAD_DIVIDE] ;
        } else if (key == HID_KEY_KEYPAD_DECIMAL) {
            ascii = intlMap.keypadDecimal ;
        } else {
            ascii = keyMapKeypadNum[key - HID_KEY_KEYPAD_DIVIDE] ;
        }
    } else {
        bool shift = (modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0 ;

        for(i = 0; intlMap.mapOther[i].code >= 0; i++) {
            if (intlMap.mapOther[i].code == key && intlMap.mapOther[i].shift == shift) {
                ascii = intlMap.mapOther[i].character ;
            }
        }
    }

    return ascii ;
}
