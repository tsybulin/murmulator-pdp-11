#ifndef _CONS_H_
#define _CONS_H_

#include "stdint.h"

// key values for control keys in ASCII range
#define KEY_BACKSPACE   0x08
#define KEY_TAB         0x09
#define KEY_ENTER       0x0d
#define KEY_ESC         0x1b
#define KEY_DELETE      0x7f

// key values for special (non-ASCII) control keys (returned by keyboard_map_key_ascii)
#define KEY_UP		    0x80
#define KEY_DOWN	    0x81
#define KEY_LEFT	    0x82
#define KEY_RIGHT	    0x83
#define KEY_INSERT	    0x84
#define KEY_HOME	    0x85
#define KEY_END		    0x86
#define KEY_PUP		    0x87
#define KEY_PDOWN	    0x88
#define KEY_PAUSE       0x89
#define KEY_PRSCRN      0x8a
#define KEY_F1      	0x8c
#define KEY_F2      	0x8d
#define KEY_F3      	0x8e
#define KEY_F4      	0x8f
#define KEY_F5      	0x90
#define KEY_F6      	0x91
#define KEY_F7      	0x92
#define KEY_F8      	0x93
#define KEY_F9      	0x94
#define KEY_F10     	0x95
#define KEY_F11     	0x96
#define KEY_F12     	0x97

#define ATTR_COLOR     0x01
#define ATTR_BGCOLOR   0x02

void cons_init() ;
void cons_cls() ;
void cons_put_char_vt100(char c) ;

void cons_printf(const char *__restrict format, ...) ;

#endif
