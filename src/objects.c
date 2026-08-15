/* objects.c: object functions for git */

#include "objects.h"
#include "utils.h"
#include "zlib.h"
#include "sha1.h"
#include "kvlm.h"
#include "ref.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *type_names[] = {"blob", "commit", "tag", "tree"};

/* Functions */

/**
 * object_type_name - returns the string representation of an object type
 * 
 * @param type  The enum value representing the Git object type.
 *
 * @return Constant string name of the object type.
 **/
const char *object_type_name(object_type type){
    return type_names[type];
}

/**
 * object_type_from_name - parses an object type enum from a string name
 * 
 * @param name  The string representation of the object type to parse.
 * @param out   Pointer to an object_type variable where the result is stored.
 *
 * @return true if the string corresponds to a valid object type,
 *         false otherwise.
 **/
bool object_type_from_name(const char *name, object_type *out){
    for(int i = 0; i < 4; i++){
        if (streq(name, type_names[i])){
            *out = (object_type)i;
            return true;
        }
    }
    return false;
}

/**
 * object_new - allocates and initializes a new Object structure
 * 
 * @param type  The object_type enum specifying the Git object type.
 * @param data  Pointer to the byte buffer containing object contents.
 * @param size  Length of the data buffer in bytes.
 *
 * @return Pointer to the newly allocated Object instance.
 **/
Object *object_new(object_type type, char *data, size_t size){
    Object *obj = safe_calloc(sizeof(Object), 1);
    obj->type = type;
    obj->size = size;
    obj->data = safe_calloc(sizeof(char), size);
    memcpy(obj->data, data, size);
    return obj;
}

/**
 * object_destroy - frees memory associated with an Object instance
 * 
 * @param obj  Pointer to the Object instance to be destroyed.
 **/
void object_destroy(Object *obj){
    if (!obj) return;
    if (obj->data){
        free(obj->data);
    }
    free(obj);
}

/**
 * zlib_inflate_all - decompresses a complete zlib stream from an open file
 * 
 * Reads all compressed data from the given file stream into memory, initializes
 * a zlib stream, and dynamically inflates the contents into an allocated buffer.
 * 
 * @note used the following websites for reference:
 *      * https://www.zlib.net/manual.html 
 *      * https://github.com/madler/zlib/blob/master/examples/zpipe.c 
 * 
 * @param f        Open FILE pointer containing compressed zlib data.
 * @param out_len  Pointer to a size_t variable where the length of the 
 *                 decompressed buffer will be written.
 *
 * @return Pointer to a newly allocated buffer containing uncompressed data,
 *         or NULL if decompression fails or stream is corrupted.
 **/
static unsigned char *zlib_inflate_all(FILE *f, size_t *out_len){
    fseek(f, 0, SEEK_END);
    long comp_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *comp = safe_malloc((size_t)comp_size, 1);
    fread(comp, 1, (size_t)comp_size, f);

    size_t cap = (size_t)comp_size * 4 + 64;
    unsigned char *out = safe_malloc(cap, 1);
    
    z_stream strm = {0};
    inflateInit(&strm);
    strm.next_in = comp;
    strm.avail_in = (uInt)comp_size;

    size_t total = 0;
    int ret;
    do {
        if (total == cap){
            cap *= 2; 
            out = safe_realloc(out, cap); 
        }
        strm.next_out = out + total;
        strm.avail_out = (uInt)(cap - total);
        ret = inflate(&strm, Z_NO_FLUSH);
        total = cap - strm.avail_out;
    } while (ret == Z_OK);

    inflateEnd(&strm);
    free(comp);

    if (ret != Z_STREAM_END){
        free(out);
        return NULL;
    }
    *out_len = total;
    return out;
}

/**
 * object_read - reads and inflates a Git object from disk by its SHA-1 hash
 * 
 * Locates the object file within the repository's 'Objects' directory using its
 * 40-character SHA-1 identifier. Decompresses the file via zlib, parses the
 * header format ("<type> <size>\0<data>"), validates the size against the actual
 * payload size, and returns a newly constructed Object.
 * 
 * @param repo  Pointer to the target Repository struct.
 * @param sha   The 40-character hexadecimal SHA-1 string identifying the object.
 *
 * @return Pointer to the constructed Object struct on success,
 *         NULL if the file does not exist, is malformed, or fails decompression.
 **/
Object *object_read(Repository *repo, const char *sha){
    if (!repo || !sha || strlen(sha) < 3) return NULL;

    char prefix[3] = {sha[0], sha[1], '\0'};
    char *path = repo_file(repo, false, "objects", prefix, sha + 2, NULL);

    if (!file_exists(path)){
        return NULL;
    }

    FILE *f = safe_fopen(path, "rb");
    free(path);

    size_t out_len;
    unsigned char *raw = zlib_inflate_all(f, &out_len);
    fclose(f);
    if (!raw) return NULL;

    char *space = memchr(raw, ' ', out_len);
    if (!space){
        free(raw);
        return NULL;
    }
    *space = '\0';

    object_type type;
    if (!object_type_from_name((char *)raw, &type)){
        free(raw);
        return NULL;
    }

    char *nul = memchr(space + 1, '\x00', out_len - (size_t)(space + 1 - (char *)raw));
    if (!nul){
        free(raw);
        return NULL;
    }

    size_t declared = (size_t)atol(space + 1);
    size_t actual = out_len - (size_t)(nul + 1 - (char *)raw);
    if (declared != actual){
        fprintf(stderr, "object_read: malformed object %s: bad length\n", sha);
        free(raw);
        return NULL;
    }

    Object *obj = object_new(type, nul + 1, actual);
    free(raw);
    return obj;
}

/**
 * object_write - serializes, hashes, and optionally stores a Git object on disk
 * 
 * Constructs the standard Git object format ("<type> <size>\0<data>"), computes
 * its SHA-1 hash, and if a Repository pointer is provided, compresses the payload
 * via zlib and writes it to the appropriate 'objects' subdirectory on disk.
 * 
 * @param obj   Pointer to the Object struct to serialize and write.
 * @param repo  Pointer to the Repository struct (if NULL, only computes SHA-1).
 *
 * @return Dynamically allocated 40-character hex string representing the 
 *         object's SHA-1 hash on success, or NULL on failure.
 **/
char *object_write(Object *obj, Repository *repo){
    if (!obj) return NULL;

    char str_size[32] = "";
    snprintf(str_size, sizeof(str_size), "%zu", obj->size);
    const char *tname= object_type_name(obj->type);
    size_t header_len = strlen(tname) + 1 + strlen(str_size) + 1;
    size_t total_len = header_len + obj->size;

    char *result = safe_calloc(1, total_len);
    size_t off = 0;
    memcpy(result, tname, strlen(tname)); off += strlen(tname);
    result[off++] = ' ';
    memcpy(result + off, str_size, strlen(str_size)); off += strlen(str_size);
    result[off++] = '\x00';
    memcpy(result + off, obj->data, obj->size);

    char *sha1 = safe_calloc(1, 41);
    sha1_hex((unsigned char *)result, total_len, sha1);

    if (repo){
        char prefix[3] = {sha1[0], sha1[1], '\0'};
        char *path = repo_file(repo, true, "objects", prefix, sha1 + 2, NULL);
        if (path && !file_exists(path)){
            uLongf bound = compressBound((uLong)total_len);
            unsigned char *compressed = safe_calloc(1, bound);
            compress(compressed, &bound, (unsigned char *)result, (uLong)total_len);
            FILE *f = safe_fopen(path, "wb");
            fwrite(compressed, 1, bound, f);
            fclose(f);
            free(compressed);
        }
        free(path);
    }

    free(result);
    return sha1;
}

/**
 * is_hex_string - checks if a string represents a valid partial or full hex SHA hash
 * 
 * @param s Input string to test.
 * 
 * @return True if the string is valid hex between 4 and 40 length, false otherwise.
 **/
static bool is_hex_string(const char *s){
    size_t len = strlen(s);
    if (len < 4 || len > 40) return false;
    for (size_t i = 0; i < len; i++){
        if (!isxdigit((unsigned char)s[i])){ return false; }
    }
    return true;
}

/**
 * object_resolve - populates candidate SHA hashes matching a reference or partial SHA string
 * 
 * Examines HEAD, hex string object prefixes in `.git/objects`, and directory branch/tag
 * references under `refs/`, adding all matching candidates to the provided StringSet.
 * 
 * @param repo       Pointer to the Repository context.
 * @param name       Reference name, HEAD, tag, or partial/full hexadecimal SHA string.
 * @param candidates StringSet instance where matched reference paths or SHAs are gathered.
 * 
 * @return Void.
 **/
static void object_resolve(Repository *repo, const char *name, StringSet *candidates){
    if (!name || !*name){ return; }

    if (streq(name, "HEAD")){
        char *sha = ref_resolve(repo, "HEAD");
        if (sha){
            string_set_add(candidates, sha);
            free(sha);
        }
        return;
    }

    if (is_hex_string(name)){
        char lower[41] = {0};
        size_t len = strlen(name);
        for (size_t i = 0 ; i < len; i++){
            lower[i] = tolower((unsigned char)name[i]);
        }
        char prefix[3] = {lower[0], lower[1], '\0'};
        char *path = repo_dir(repo, false, "objects", prefix, NULL);
        if (path){
            const char *rem = lower + 2;
            size_t rem_len = strlen(rem);
            DIR *d = opendir(path);
            if (d){
                for (struct dirent *e = readdir(d); e; e = readdir(d)){
                    if (streq(e->d_name, ".") || streq(e->d_name, "..")){ continue; }
                    if (strncmp(rem, e->d_name, rem_len)){
                        char full[MAX_PATH];
                        snprintf(full, sizeof(full), "%s%s", prefix, e->d_name);
                        string_set_add(candidates, full);
                    }
                }
                closedir(d);
            }
            free(path);
        }
    }

    const char *dirs[] = { "refs/tags", "refs/heads", "refs/remotes" };
    for (int i = 0; i < 3; i++){
        char *path = path_join(dirs[i], name, NULL);
        if (path){
            char *sha = ref_resolve(repo, path);
            if (sha){
                string_set_add(candidates, path);
                free(sha);
            }
            free(path);
        }
    }
}

/**
 * object_find - resolves an object name reference to its matching SHA-1 string
 * 
 * Looks up an object identifier or reference within a repository and resolves it 
 * to its corresponding hexadecimal SHA-1 string representation.
 * 
 * @param repo    Pointer to the Repository context.
 * @param name    The name, ref, or partial SHA string to search for.
 * @param type    Expected object_type filter.
 * @param follow  Whether to recursively dereference symrefs or tags.
 *
 * @return Dynamically allocated SHA-1 string matching the object,
 *         or NULL if not found.
 **/
char *object_find(Repository *repo, const char *name, object_type type, bool follow){
    StringSet shas;
    string_set_init(&shas);
    object_resolve(repo, name, &shas);

    if (shas.count == 0){
        fprintf(stderr, "object_find: no such reference '%s'\n", name);
        string_set_destroy(&shas);
        return NULL;
    }

    if (shas.count > 1){
        fprintf(stderr, "Ambiguous reference %s, Candidates are:\n", name);
        for (size_t i = 0; i < shas.count; i++){
            fprintf(stderr, " - %s\n", shas.items[i]);
        }
        string_set_destroy(&shas);
        return NULL;
    }

    char *sha = safe_strdup(shas.items[0]);
    string_set_destroy(&shas);

    if (type == GIT_ANY_TYPE) return sha;

    for (;;){
        Object *obj = object_read(repo, shas.items[0]);
        if (!obj) { free(sha); return NULL; }

        if (obj->type == type){ object_destroy(obj); return sha; }
        if (!follow){ object_destroy(obj); free(sha); return NULL; }

        char *next = NULL;
        if (obj->type == GIT_TAG){
            KVLM *kvlm = kvlm_parse(obj->data, obj->size);
            const char *inner = kvlm_get(kvlm, "object");
            if (inner) next = safe_strdup(inner);
            kvlm_destroy(kvlm);
        } else if (obj->type == GIT_COMMIT && type == GIT_TREE){
            KVLM *kvlm = kvlm_parse(obj->data, obj->size);
            const char *inner = kvlm_get(kvlm, "tree");
            if (inner) next = safe_strdup(inner);
            kvlm_destroy(kvlm);
        } else {
            object_destroy(obj);
            free(sha);
            return NULL;
        }
        object_destroy(obj);
        free(sha);
        if (!next) return NULL;
        sha = next;
    }
}

/**
 * object_hash - hashes the contents of an open file descriptor into a Git object
 * 
 * Reads the full content of the provided file descriptor, constructs an Object
 * of the specified type, and delegates to object_write to generate its SHA-1
 * hash and optionally persist it to disk if a repository is specified.
 * 
 * @param fd    File descriptor opened for reading.
 * @param type  The object_type enum to assign to the hashed content.
 * @param repo  Pointer to the Repository context (or NULL to skip saving).
 *
 * @return Dynamically allocated 40-character hexadecimal SHA-1 string,
 *         or NULL on read/stat failure.
 **/
char *object_hash(int fd, object_type type, Repository *repo){
    struct stat st;
    if (fstat(fd, &st) < 0) return NULL;

    size_t size = (size_t)st.st_size;
    char *data = safe_calloc(1, size ? size + 1 : 1);
    ssize_t n = read(fd, data, size); 
    if (n < 0 || (size_t)n != size){
        free(data);
        return NULL;
    }

    Object *obj = object_new(type, data, size);
    free(data);

    char *sha1 = object_write(obj, repo);
    object_destroy(obj);
    return sha1;
}

/**
 * cat_file - outputs the raw contents of a Git object to stdout
 * 
 * Resolves an object reference string to its SHA-1, inflates the corresponding
 * object from the repository database, and writes its uncompressed payload data
 * directly to standard output.
 * 
 * @param repo  Pointer to the active Repository struct.
 * @param name  Object name or SHA reference to display.
 * @param type  Expected object_type filter.
 *
 * @return true if the object was located, inflated, and written successfully,
 *         false otherwise.
 **/
bool cat_file(Repository *repo, const char *name, object_type type){
    char *sha1 = object_find(repo, name, type, true);
    if (!sha1) return false;

    Object *obj = object_read(repo, sha1);
    free(sha1);
    if (!obj) return false;

    fwrite(obj->data, 1, obj->size, stdout);
    object_destroy(obj);
    return true;
}

/**
 * commit_parse - parses a Git commit Object into a KVLM structure
 * 
 * A convenience wrapper around kvlm_parse that validates the Object type 
 * and directly passes its internal data buffer for parsing.
 * 
 * @param obj  Pointer to the Git Object (must be of type GIT_COMMIT).
 *
 * @return Pointer to the constructed KVLM struct, or NULL on invalid input.
 **/
KVLM *commit_parse(Object *obj){
    if (!obj || obj->type != GIT_COMMIT) return NULL;
    return kvlm_parse(obj->data, obj->size);
}

/**
 * commit_from_kvlm - serializes a KVLM struct into a standard Git Object
 * 
 * Takes an existing KVLM structure, serializes it to raw Git format, and 
 * wraps the resulting payload in a newly allocated Git Object marked as a commit.
 * 
 * @param kvlm  Pointer to the KVLM structure containing commit data.
 *
 * @return Pointer to the newly constructed Git Object.
 **/
Object *commit_from_kvlm(KVLM *kvlm){
    return object_from_kvlm(GIT_COMMIT, kvlm);
}

/**
 * object_to_tree - converts a raw Git Object into a Tree struct
 * 
 * Validates that the provided object is of type GIT_TREE, then delegates
 * the parsing of the object's raw payload to tree_parse().
 * 
 * @param obj Pointer to the Object struct to convert.
 * 
 * @return A pointer to the newly allocated Tree struct, or NULL if the object
 *         is invalid, NULL, or not a tree.
 **/
Tree *object_to_tree(Object *obj){
    if(!obj || obj->type != GIT_TREE) return NULL;
    return tree_parse(obj->data, obj->size);
}

/**
 * tree_to_object - converts a Tree struct into a raw Git Object
 * 
 * Serializes the Tree struct into the Git binary format, then wraps that 
 * binary data into a newly allocated Object struct initialized with the GIT_TREE type.
 * 
 * @param tree Pointer to the Tree struct to convert.
 * 
 * @return A pointer to the newly allocated Object struct, or NULL on failure.
 **/
Object *tree_to_object(Tree *tree){
    if (!tree) return NULL;
    size_t len;
    char *data = tree_serialize(tree, &len);
    Object *obj = object_new(GIT_TREE, data, len);
    free(data);
    return obj;
}

/**
 * object_from_kvlm - constructs a Git Object from a KVLM structure
 * 
 * Serializes the given KVLM structure to raw byte buffer representation 
 * and wraps it inside a new Object structure assigned with the specified type.
 * 
 * @param type Object type enum to set for the new Object.
 * @param kvlm Pointer to the KVLM structure to serialize.
 * 
 * @return Pointer to the newly allocated Object struct.
 **/
Object *object_from_kvlm(object_type type, KVLM *kvlm){
    size_t len;
    char *data = kvlm_serialize(kvlm, &len);
    Object *obj = object_new(type, data, len);
    free(data);
    return obj;
}