#include "ini.h"

#include "graphics.h"
#include "f_util.h"
#include <string.h>
#include <ctype.h>
#include <pico/stdlib.h>

#define INI_MAX_LINE 40
#define MAX_SECTION 10
#define MAX_NAME 20
#define HANDLER(u, s, n, v) handler(u, s, n, v)
#define INI_INLINE_COMMENT_PREFIXES ";"
#define INI_START_COMMENT_PREFIXES ";#"

typedef struct {
    const char* ptr;
    size_t num_left;
} ini_parse_string_ctx ;

static char* ini_rstrip(char* s) {
    char* p = s + strlen(s) ;
    while (p > s && isspace((unsigned char)(*--p))) {
        *p = '\0';
    }
    return s;
}

static char* ini_lskip(const char* s) {
    while (*s && isspace((unsigned char)(*s))) {
        s++;
    }
    return (char*)s;
}

static char* ini_find_chars_or_comment(const char* s, const char* chars) {
    int was_space = 0;
    while (*s && (!chars || !strchr(chars, *s)) &&
           !(was_space && strchr(INI_INLINE_COMMENT_PREFIXES, *s))) {
        was_space = isspace((unsigned char)(*s));
        s++;
    }
    return (char*)s;
}

int ini_parse_stream(ini_reader reader, void* stream, ini_handler handler, void* user) {
    char line[INI_MAX_LINE];
    size_t max_line = INI_MAX_LINE;
    char section[MAX_SECTION] = "";
    char prev_name[MAX_NAME] = "";

    char* start;
    char* end;
    char* name;
    char* value;
    int lineno = 0;
    int error = 0;


    /* Scan through stream line by line */
    while (reader(line, (int)max_line, stream) != NULL) {
        lineno++;

        start = line;
        start = ini_lskip(ini_rstrip(start));

        if (strchr(INI_START_COMMENT_PREFIXES, *start)) {
            /* Start-of-line comment */
        } else if (*start == '[') {
            /* A "[section]" line */
            end = ini_find_chars_or_comment(start + 1, "]");
            if (*end == ']') {
                *end = '\0';
                strncpy(section, start + 1, sizeof(section));
                *prev_name = '\0';
                if (!HANDLER(user, section, NULL, NULL) && !error) {
                    error = lineno;
                }
            } else if (!error) {
                /* No ']' found on section line */
                error = lineno;
            }
        } else if (*start) {
            /* Not a comment, must be a name[=:]value pair */
            end = ini_find_chars_or_comment(start, "=:");
            if (*end == '=' || *end == ':') {
                *end = '\0';
                name = ini_rstrip(start);
                value = end + 1;
                end = ini_find_chars_or_comment(value, NULL);
                if (*end) {
                    *end = '\0';
                }
                value = ini_lskip(value);
                ini_rstrip(value);

                /* Valid name[=:]value pair found, call handler */
                strncpy(prev_name, name, sizeof(prev_name));
                if (!HANDLER(user, section, name, value) && !error)
                    error = lineno;
            }
            else if (!error) {
                /* No '=' or ':' found on name[=:]value line */
                error = lineno;
            }
        }

        if (error)
            break;
    }

    return error;
}

int ini_parse_file(FIL* file, ini_handler handler, void* user) {
    return ini_parse_stream((ini_reader)f_gets, file, handler, user);
}

FRESULT ini_parse(const char* filename, ini_handler handler, void* user) {
    FIL file;
    int error;

    FRESULT result = f_open(&file, filename, FA_READ);
    if (result != FR_OK) {
		gprintf("f_open(%s) error: %s (%d)", filename, FRESULT_str(result), result);
        while(1) ;
    }
    error = ini_parse_file(&file, handler, user);
    f_close(&file);
    return error;
}

static char* ini_reader_string(char* str, int num, void* stream) {
    ini_parse_string_ctx* ctx = (ini_parse_string_ctx*)stream;
    const char* ctx_ptr = ctx->ptr;
    size_t ctx_num_left = ctx->num_left;
    char* strp = str;
    char c;

    if (ctx_num_left == 0 || num < 2)
        return NULL;

    while (num > 1 && ctx_num_left != 0) {
        c = *ctx_ptr++;
        ctx_num_left--;
        *strp++ = c;
        if (c == '\n')
            break;
        num--;
    }

    *strp = '\0';
    ctx->ptr = ctx_ptr;
    ctx->num_left = ctx_num_left;
    return str;
}

int ini_parse_string(const char* string, ini_handler handler, void* user) {
    ini_parse_string_ctx ctx;

    ctx.ptr = string;
    ctx.num_left = strlen(string);
    return ini_parse_stream((ini_reader)ini_reader_string, &ctx, handler, user);
}
