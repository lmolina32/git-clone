/* utils.h: utility functions*/

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

/* Macros */

#define MAX_NAME (1<<8)

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

/* Memory & IO */

static inline FILE *safe_fopen(const char *f, const char *s){
    FILE *fp = fopen((f), (s));
    FILE_CHECK(fp, f);
    return fp; 
}

static inline void *safe_malloc(size_t t, size_t s){
    void *ptr = malloc((t) * (s)); 
    MALLOC_CHECK(ptr); 
    return ptr; 
} 

static inline void *safe_calloc(size_t size, size_t count){ 
    void *ptr = calloc((count), (size)); 
    MALLOC_CHECK(ptr); 
    return ptr; 
}

static inline void *safe_realloc(void *p, size_t s){
    void *ptr = realloc((p), (s));
    MALLOC_CHECK(ptr);
    return ptr;
}

static inline void *safe_strdup(const char *s){ 
    char *ptr = strdup(s); 
    MALLOC_CHECK(ptr); 
    return ptr; 
}

/* Structures */

typedef struct {
    char    **items;
    size_t    count;
    size_t    capacity; 
} StringSet;


typedef struct {
    char    *data;
    size_t   len;
    size_t   cap;
} DynBuf;

/* Functions */

char *path_join(const char *s1, ...);
bool is_directory(const char *path);
bool file_exists(const char *path);
bool mkdir_p(const char *path, mode_t mode); 
bool is_directory_empty(const char *path);
bool remove_directory(const char *path);
void string_set_init(StringSet *set);
bool string_set_contains(StringSet *set, const char *s);
void string_set_add(StringSet *set, const char *s);
void string_set_destroy(StringSet *set);
void dynbuf_init(DynBuf *db);
void dynbuf_append(DynBuf *db, const void *ptr, size_t n);
void dynbuf_destroy(DynBuf *db);

/* Miscellaneous */

#define chomp(s)            if (strlen(s)) { s[strlen(s) - 1] = 0; }
#define chomp_quotes(s)     if (strlen(s)) { s[strlen(s) - 1] = 0; s[0] = 0; }
#define min(a, b)           ((a) < (b) ? (a) : (b))
#define streq(a, b)         (strcmp(a, b) == 0)
#define strneq(a, b, s)     (strncmp(a, b, s) == 0)

#endif