#ifndef DEBUG_H
#define DEBUG_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <execinfo.h>

char* dbg_identifier;
static char* dbg_to_yellow = "\033[33;1m";
static char* dbg_to_red = "\033[31;1m";
static char* dbg_to_normal = "\033[0m";

#define STACK_FRAME_SIZE 2

static inline void debug(char* format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s [*] ", dbg_identifier);
    vfprintf(stderr, format, args);
    va_end(args);
#endif
}

static inline void warn(char* format, ...) {
    void* stackframes[STACK_FRAME_SIZE];
    int n_traces = backtrace(stackframes, STACK_FRAME_SIZE);
    va_list args;
    va_start(args, format);
    char** symbols = backtrace_symbols(stackframes, STACK_FRAME_SIZE);
    fprintf(stderr, "\n");
    for (int i = 1; i < n_traces; i++) {
        fprintf(stderr, "%s\n", symbols[i]);
    }
    fprintf(stderr, "%s[WARNING]%s[%s] ", dbg_to_yellow, dbg_to_normal, dbg_identifier);
    vfprintf(stderr, format, args);
    va_end(args);
    free(symbols);
}

static inline void panic(char* format, ...) {
    void* stackframes[STACK_FRAME_SIZE];
    int n_traces = backtrace(stackframes, STACK_FRAME_SIZE);
    va_list args;
    va_start(args, format);
    char** symbols = backtrace_symbols(stackframes, STACK_FRAME_SIZE);
    fprintf(stderr, "\n");
    for (int i = 0; i < n_traces; i++) {
        fprintf(stderr, "%s\n", symbols[i]);
    }
    fprintf(stderr, "%s[PANIC]%s[%s] ", dbg_to_red, dbg_to_normal, dbg_identifier);
    vfprintf(stderr, format, args);
    va_end(args);
    free(symbols);
}

#endif