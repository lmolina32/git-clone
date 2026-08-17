/* Include.h */

#ifndef INCLUDE_H
#define INCLUDE_H

#include "repository.h"

#include <stdint.h>
#include <stddef.h>

/* Structures */

typedef struct {
    uint32_t  ctime_s;
    uint32_t  ctime_ns;
    uint32_t  mtime_s;
    uint32_t  mtime_ns;
    uint32_t  dev;
    uint32_t  ino;
    uint16_t  mode_type;
    uint16_t  mode_perms;
    uint32_t  uid;
    uint32_t  gid;
    uint32_t  fsize;
    char      sha[41];
    bool      flag_assume_valid;
    uint16_t  flag_stage;
    char     *name;
} IndexEntry;

typedef struct {
    uint32_t    version;
    IndexEntry *entries;
    size_t      count;
    size_t      capacity;
} GitIndex;

/* Functions */

GitIndex  *index_read(Repository *repo);
void       index_destroy(GitIndex *index);

#endif