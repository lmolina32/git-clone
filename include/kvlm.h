/* kvlm.h */

#ifndef KVLM_H
#define KVLM_H

#include "repository.h"
#include "utils.h"

#include <stddef.h>

/* Structures */

typedef struct {
    char    *key;
    char   **values;
    size_t   value_count;
    size_t   value_cap; 
} KVLMEntry;

typedef struct {
    KVLMEntry *entries;
    size_t     count;
    size_t     capacity;
} KVLM;

/* Functions */

KVLM              *kvlm_new();
void               kvlm_destroy(KVLM *kvlm);

KVLM              *kvlm_parse(const char *raw, size_t len);
char              *kvlm_serialize(KVLM *kvlm, size_t *out_len);

void               kvlm_set(KVLM *kvlm, const char *key, const char *value);
const char        *kvlm_get(KVLM *kvlm, const char *key);
const char *const *kvlm_get_all(KVLM *kvlm, const char *key, size_t *count);
bool               log_graphviz(Repository *repo, const char *sha, StringSet *seen);

#endif