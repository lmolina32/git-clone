/* kvlm.c: key-value list with message parser/serializer */

#include "kvlm.h"
#include "objects.h"
#include "repository.h"
#include "utils.h"

#include <string.h>
#include <stdio.h>

/* Functions */

/**
 * kvlm_new - allocates and initializes a new KVLM structure
 * 
 * @return Pointer to the newly allocated KVLM struct.
 **/
KVLM *kvlm_new(){
    KVLM *kvlm = safe_calloc(sizeof(KVLM), 1);
    return kvlm;
}

/**
 * kvlm_destroy - frees a KVLM structure and all associated memory
 * 
 * @param kvlm  Pointer to the KVLM struct to be destroyed.
 **/
void kvlm_destroy(KVLM *kvlm){
    if (!kvlm) return; 
    for (size_t i = 0; i < kvlm->count; i++){
        free(kvlm->entries[i].key);
        for (size_t j = 0; j < kvlm->entries[i].value_count; j++){
            free(kvlm->entries[i].values[j]);
        }
        free(kvlm->entries[i].values);
    }
    free(kvlm->entries);
    free(kvlm);
}

/**
 * find_entry - searches for an existing entry matching the given key
 * 
 * @param kvlm  Pointer to the KVLM struct to search.
 * @param key   The string key to search for, or NULL for the message block.
 *
 * @return Pointer to the matched KVLMEntry, or NULL if not found.
 **/
static KVLMEntry *find_entry(KVLM *kvlm, const char *key){
    if (!kvlm) return NULL;
    for (size_t i = 0; i < kvlm->count; i++){
        char *k = kvlm->entries[i].key;
        if (key == NULL ? k == NULL : (k != NULL && streq(k, key))){
            return &kvlm->entries[i];
        }
    }
    return NULL;
}

/**
 * kvlm_set - adds a value to a specific key in the KVLM struct
 * 
 * Finds the entry for the given key (creating it if it doesn't exist) and 
 * appends the value to its list of values. Dynamically scales the internal 
 * entry array and value arrays as needed. Passing NULL as the key sets the 
 * message block.
 * 
 * @param kvlm   Pointer to the target KVLM struct.
 * @param key    The string key (e.g., "tree", "parent"), or NULL for the message.
 * @param value  The string value to store.
 **/
void kvlm_set(KVLM *kvlm, const char *key, const char *value){
    if (!kvlm || !value) return;
    KVLMEntry *e = find_entry(kvlm, key);

    if (!e){
        if (kvlm->count == kvlm->capacity){
            kvlm->capacity = kvlm->capacity ? kvlm->capacity * 2 : 8;
            kvlm->entries = safe_realloc(kvlm->entries, kvlm->capacity * sizeof(KVLMEntry));
        }
        e = &kvlm->entries[kvlm->count++];
        e->key = key ? safe_strdup(key) : NULL;
        e->values = NULL;
        e->value_count = 0;
        e->value_cap = 0;
    }

    if (e->value_count == e->value_cap){
        e->value_cap = e->value_cap ? e->value_cap * 2 : 2;
        e->values = safe_realloc(e->values, e->value_cap * sizeof(char *));
    }
    e->values[e->value_count++] = safe_strdup(value);
}

/**
 * kvlm_get - retrieves the first value associated with a given key
 * 
 * @param kvlm  Pointer to the KVLM struct.
 * @param key   The string key to look up, or NULL for the message payload.
 *
 * @return Pointer to the value string, or NULL if the key is not found.
 **/
const char *kvlm_get(KVLM *kvlm, const char *key){
    if (!kvlm) return NULL;
    KVLMEntry *e = find_entry(kvlm, key);
    return (e && e->value_count) ? e->values[0] : NULL;
}

/**
 * kvlm_get_all - retrieves all values associated with a given key
 * 
 * @param kvlm   Pointer to the KVLM struct.
 * @param key    The string key to look up.
 * @param count  Pointer to a size_t where the number of values will be stored.
 *
 * @return Array of string pointers containing the values, or NULL if not found.
 **/
const char *const *kvlm_get_all(KVLM *kvlm, const char *key, size_t *count){
    if (!kvlm){ *count = 0; return NULL; }
    KVLMEntry *e = find_entry(kvlm, key);
    if (!e){ *count = 0; return NULL; }
    *count = e->value_count;
    return (const char * const *)e->values;
}

/**
 * drop_continuation_spaces - processes multi-line values from raw Git objects
 * 
 * Git object headers can span multiple lines if subsequent lines begin with a 
 * space. This function strips out the newline-space continuations to reconstruct 
 * the original contiguous string value.
 * 
 * @param s        The raw string payload starting at the value.
 * @param len      The length of the raw string payload.
 * @param out_len  Pointer to a size_t where the cleaned string length is stored.
 *
 * @return A newly allocated string with continuation spaces removed.
 **/
static char *drop_continuation_spaces(const char *s, size_t len, size_t *out_len){
    if (!s) { *out_len = 0; return NULL; }
    char *out = safe_calloc(1, len + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++){
        out[j++] = s[i];
        if (s[i] == '\n' && (i + 1) < len && s[i + 1] == ' '){
            i++;
        }
    }
    *out_len = j;
    return out;
}

/**
 * kvlm_parse - parses a raw Git object string into a KVLM structure
 * 
 * Reads through a raw Git object payload (like a commit or tag), splitting 
 * it into key-value pairs based on spaces and newlines. Handles multi-line 
 * values (continuation spaces) and extracts the trailing free-text message.
 * 
 * @param raw  The raw, decompressed payload data of the Git object.
 * @param len  The total size of the raw payload in bytes.
 *
 * @return Pointer to the newly constructed KVLM struct.
 **/
KVLM *kvlm_parse(const char *raw, size_t len){
    KVLM *kvlm = kvlm_new();
    size_t start = 0;

    for (;;){
        long spc = -1, nl = -1; 
        for (size_t i = start; i < len; i++){
            if (raw[i] == '\n'){ nl = (long)i; break; }
            if (spc < 0 && raw[i] == ' '){ spc = (long)i; }
        }
        if (nl < 0) nl = (long)len;

        /* Base Case: blank line reached */
        if (spc < 0 || nl < spc){
            size_t msg_start = start + 1 <= len ? start + 1 : len;
            size_t msg_len = len - msg_start;
            char *msg = safe_calloc(1, msg_len + 1);
            memcpy(msg, raw + msg_start, msg_len);
            kvlm_set(kvlm, NULL, msg);
            free(msg);
            break;
        }

        /* key */
        size_t key_len = (size_t)spc - start;
        char *key = safe_calloc(1, key_len + 1);
        memcpy(key, raw + start, key_len);

        /* Find new line not starting with space*/
        size_t end = (size_t)nl;
        while (end + 1 < len && raw[end + 1] == ' '){
            size_t next_nl = end + 1;
            while (next_nl < len && raw[next_nl] != '\n') next_nl++;
            end = next_nl;
        }

        size_t value_start = (size_t)spc + 1;
        size_t raw_val_len = end - value_start;
        size_t clean_len;
        char *value = drop_continuation_spaces(raw + value_start, raw_val_len, &clean_len);

        kvlm_set(kvlm, key, value);
        free(key);
        free(value);

        start = end + 1;
        if (start >= len) break;
    }

    return kvlm;
}

/**
 * kvlm_serialize - converts a KVLM structure back into a raw Git string format
 * 
 * Iterates through all key-value entries in the KVLM struct and formats them 
 * into standard Git object syntax, including injecting continuation spaces 
 * for multi-line values and appending the final message block.
 * 
 * @param kvlm     Pointer to the KVLM struct to serialize.
 * @param out_len  Pointer to a size_t where the resulting string length is stored.
 *
 * @return A newly allocated string containing the serialized Git object data.
 **/
char *kvlm_serialize(KVLM *kvlm, size_t *out_len){
    DynBuf buf;
    dynbuf_init(&buf);

    const char *message = NULL;

    for (size_t i = 0; i < kvlm->count; i++){
        KVLMEntry *e = &kvlm->entries[i];
        if (e->key == NULL){
            if (e->value_count) message = e->values[0];
            continue;
        }
        for (size_t j = 0; j < e->value_count; j++){
            dynbuf_append(&buf, e->key, strlen(e->key));
            dynbuf_append(&buf, " ", 1);

            const char *v = e->values[j];
            size_t vlen = strlen(v);
            for (size_t k = 0; k < vlen; k++){
                dynbuf_append(&buf, &v[k], 1);
                if (v[k] == '\n') dynbuf_append(&buf, " ", 1);
            }
            dynbuf_append(&buf, "\n", 1);
        }
    }

    dynbuf_append(&buf, "\n", 1);
    if (message) dynbuf_append(&buf, message, strlen(message));

    *out_len = buf.len;
    return buf.data;
}

/**
 * escape_graphviz_label - escapes special characters for Graphviz DOT format
 * 
 * @param s  The string to escape.
 *
 * @return A newly allocated, appropriately escaped string.
 **/
static char *escape_graphviz_label(const char *s) {
    size_t len = strlen(s);
    char *out = safe_malloc(len * 2 + 1, 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\' || s[i] == '"') out[j++] = '\\';
        out[j++] = s[i];
    }
    out[j] = '\0';
    return out;
}

/**
 * log_graphviz - recursively traverses and prints a commit graph in DOT syntax
 * 
 * Starting from a given commit SHA, this function parses commits, extracts 
 * their metadata, prints Graphviz nodes and edges, and recursively traverses 
 * up through parent commits. Uses a StringSet to avoid revisiting commits.
 * 
 * @param repo  Pointer to the target Repository struct.
 * @param sha   The 40-character SHA-1 hash of the starting commit.
 * @param seen  Pointer to a StringSet used to track already-visited commits.
 *
 * @return true on complete success, false if a commit cannot be read or parsed.
 **/
bool log_graphviz(Repository *repo, const char *sha, StringSet *seen){
    if (string_set_contains(seen, sha)){
        return true;
    }
    string_set_add(seen, sha);

    Object *obj = object_read(repo, sha);
    if (!obj || obj->type != GIT_COMMIT){
        fprintf(stderr, "log_graphviz: %s is not a commit\n", sha);
        object_destroy(obj);
        return false;
    }

    KVLM *kvlm = commit_parse(obj);
    const char *raw_msg = kvlm_get(kvlm, NULL);

    char *message = safe_strdup(raw_msg ? raw_msg : "");
    size_t mlen = strlen(message);
    while (mlen > 0 && (message[mlen - 1] == '\n' || message[mlen - 1] == ' '))
        message[--mlen] = '\0';
    char *nl = strchr(message, '\n');
    if (nl) *nl = '\0';

    char *escaped = escape_graphviz_label(message);
    free(message);

    char short_sha[8];
    strncpy(short_sha, sha, 7);
    short_sha[7] = '\0';

    printf("  c_%s [label=\"%s: %s\"]\n", sha, short_sha, escaped);
    free(escaped);

    size_t parent_count;
    const char *const *parents = kvlm_get_all(kvlm, "parent", &parent_count);

    bool ok = true;
    for (size_t i = 0; i < parent_count; i++) {
        printf("  c_%s -> c_%s;\n", sha, parents[i]);
        if (!log_graphviz(repo, parents[i], seen)) ok = false;
    }

    kvlm_destroy(kvlm);
    object_destroy(obj);
    return ok;
}