#ifndef DEBUG_H
#define DEBUG_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <execinfo.h>

char* dbg_identifier;
static char* debug_color = "\033[94m";
static char* warn_color = "\033[33;1m";
static char* panic_color = "\033[31;1m";
static char* reset_color = "\033[0m";

#define STACK_FRAME_SIZE 3
#define FORMAT_MAXLEN 512

static inline void debug(char* format, ...) {
#ifdef DEBUG
    char internal_format[FORMAT_MAXLEN];
    va_list args;
    va_start(args, format);
    snprintf(internal_format, FORMAT_MAXLEN, "%s[*]%s %s %s", debug_color, reset_color, dbg_identifier, format);
    vfprintf(stderr, internal_format, args);
    va_end(args);
#endif
}

static inline void warn(char* format, ...) {
    void* stackframes[STACK_FRAME_SIZE];
    int n_traces = backtrace(stackframes, STACK_FRAME_SIZE);
    char** symbols = backtrace_symbols(stackframes, STACK_FRAME_SIZE);
    fprintf(stderr, "\n");
    for (int i = n_traces - 1; i > 0; i--) {
        fprintf(stderr, "%s\n", symbols[i]);
    }

    char internal_format[FORMAT_MAXLEN];
    va_list args;
    va_start(args, format);
    snprintf(internal_format, FORMAT_MAXLEN, "%s[WARNING]%s %s %s", warn_color, reset_color, dbg_identifier, format);
    vfprintf(stderr, internal_format, args);
    va_end(args);
    free(symbols);
}

static inline void panic(char* format, ...) {
    void* stackframes[STACK_FRAME_SIZE];
    int n_traces = backtrace(stackframes, STACK_FRAME_SIZE);
    char** symbols = backtrace_symbols(stackframes, STACK_FRAME_SIZE);
    fprintf(stderr, "\n");
    for (int i = n_traces - 1; i > 0; i--) {
        fprintf(stderr, "%s\n", symbols[i]);
    }

    char internal_format[FORMAT_MAXLEN];
    va_list args;
    va_start(args, format);
    snprintf(internal_format, FORMAT_MAXLEN, "%s[PANIC]%s %s %s", panic_color, reset_color, dbg_identifier, format);
    vfprintf(stderr, internal_format, args);
    va_end(args);
    free(symbols);
    exit(EXIT_FAILURE);
}

#endif