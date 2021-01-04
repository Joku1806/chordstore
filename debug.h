#ifndef DEBUG_H
#define DEBUG_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

static char* dbg_identifier = "";
static char* dbg_to_yellow = "\033[33;1;0m";
static char* dbg_to_red = "\033[31;1;0m";
static char* dbg_to_normal = "\033[0m";

static inline void debug(char* format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s [*] %s: ", dbg_identifier, __func__);
    vfprintf(stderr, format, args);
    va_end(args);
#endif
}

static inline void warn(char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s %s[WARNING]%s %s in %s:%d - ", dbg_identifier, dbg_to_yellow, dbg_to_normal, __func__, __FILE__, __LINE__);
    vfprintf(stderr, format, args);
    va_end(args);
}

static inline void panic(char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s %s[PANIC]%s %s in %s:%d - ", dbg_identifier, dbg_to_red, dbg_to_normal, __func__, __FILE__, __LINE__);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

#endif