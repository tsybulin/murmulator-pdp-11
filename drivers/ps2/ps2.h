#ifndef _PS2_H_
#define _PS2_H_

#ifdef __cplusplus
 extern "C" {
#endif


#include "strings.h"
#include "stdio.h"

#include <stdbool.h>
#include <stdint.h>

#include "hid.h"

#ifndef KBD_CLOCK_PIN
#define KBD_CLOCK_PIN    (0)
#endif
#ifndef KBD_DATA_PIN
#define KBD_DATA_PIN    (1)
#endif
#define KBD_BUFFER_SIZE 16

#define PS2_LED_SCROLL_LOCK 1
#define PS2_LED_NUM_LOCK    2
#define PS2_LED_CAPS_LOCK   4

extern uint8_t kbloop;

typedef void (*ps2_handler_t)(uint8_t, uint8_t) ;

void keyboard_init(ps2_handler_t handler);
void keyboard_toggle_led(uint8_t led);
int16_t keyboard_send(uint8_t data);

#ifdef __cplusplus
 }
#endif

#endif
