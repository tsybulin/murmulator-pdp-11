#ifndef _CONS_H_
#define _CONS_H_

#include "stdint.h"

#define ATTR_COLOR     0x01
#define ATTR_BGCOLOR   0x02

void cons_init() ;
void cons_cls() ;
uint8_t cons_get_attribute(uint8_t x, uint8_t y, uint8_t attr) ;
void cons_set_attribute(uint8_t x, uint8_t y, uint8_t attr, uint8_t val) ;
void cons_put_char(char c) ;
void cons_draw_line(const char* title, uint32_t row) ;
void cons_scroll() ;
void cons_printf(const char *__restrict format, ...) ;

#endif
