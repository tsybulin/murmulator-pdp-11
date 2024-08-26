#ifndef _CONS_H_
#define _CONS_H_

#include "stdint.h"

void cons_init() ;
void cons_put_char(char c) ;
void cons_draw_line(const char* title, uint32_t row) ;
void cons_scroll() ;

#endif
