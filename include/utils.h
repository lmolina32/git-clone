/* utils.h: utility functions*/

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Macros */

#define MAX_NAME 1<<8

#define MALLOC_CHECK(ptr) \
    do { \
        if (ptr == NULL){ \
            fprintf(stderr, "Malloc return NULL pointer, cannot continue compiling\n"); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define FILE_CHECK(ptr, file) \
    do { \
        if (ptr == NULL){ \
            fprintf(stderr, "%s %s\n", strerror(errno), file); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

/* Print indent */

#define print_indent(i) \
    do { \
        for (int _i = 0; _i < ((i) * 4); _i++) \
        printf(" "); \
    } while (0)

/* File open */

#define safe_fopen(f, s) ({ \
    FILE *_fp = fopen((f), (s)); \
    FILE_CHECK(_fp, f); \
    _fp; \
})

/* Malloc */

#define safe_malloc(t, s) ({ \
    void *_ptr = malloc((t) * (s)); \
    MALLOC_CHECK(_ptr); \
    _ptr; \
}) 

#define safe_calloc(t, s) ({ \
    void *_ptr = calloc((s), (t)); \
    MALLOC_CHECK(_ptr); \
    _ptr; \
})

/* strdup */

#define safe_strdup(s) ({ \
    char *_ptr = strdup(s); \
    MALLOC_CHECK(_ptr); \
    _ptr; \
})

/* Miscellaneous */

#define chomp(s)            if (strlen(s)) { s[strlen(s) - 1] = 0; }
#define chomp_quotes(s)      if (strlen(s)) { s[strlen(s) -1] = 0; s[0] = 0;}
#define min(a, b)           ((a) < (b) ? (a) : (b))
#define streq(a, b)         (strcmp(a, b) == 0)

#endif