#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"

typedef char* (*ini_reader)(char* str, int num, void* stream);
typedef int (*ini_handler)(void* user, const char* section, const char* name, const char* value) ;

FRESULT ini_parse(const char* filename, ini_handler handler, void* user);

#ifdef __cplusplus
}
#endif
