#ifndef _CONS_H_
#define _CONS_H_

#include "stdint.h"

#define ATTR_COLOR     0x01
#define ATTR_BGCOLOR   0x02

void cons_init() ;
void cons_cls() ;
void cons_put_char_vt100(char c) ;
void cons_draw_line(const char* title, uint32_t row) ;

void cons_printf(const char *__restrict format, ...) ;

#endif
