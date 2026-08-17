/* tree.c: Tree object parsing and serialization */

#include "tree.h"
#include "objects.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


/**
 * tree_new - creates a new, empty Tree struct
 * 
 * @return A pointer to the newly allocated Tree, or NULL on failure.
 **/
Tree *tree_new(){
    return safe_calloc(sizeof(Tree), 1);
}

/**
 * tree_destroy - frees memory associated with a Tree struct
 * 
 * @param tree Pointer to the Tree struct to destroy. Safe to pass NULL.
 **/
void tree_destroy(Tree *t){
    if (!t) return;
    for (size_t i = 0; i < t->count; i++){
        free(t->entries[i].path);
    }
    free(t->entries);
    free(t);
}

/**
 * tree_add_entry - adds a new entry to the Tree
 * 
 * Appends a new file, directory, or symlink entry to the Tree's internal list.
 * Automatically scales the capacity of the entries array as needed. Modes missing
 * leading zeros (e.g., "40000") will be padded to 6 characters.
 * 
 * @param tree  Pointer to the Tree struct.
 * @param mode  The octal file mode as a string (e.g., "100644", "040000").
 * @param path  The file or directory path name.
 * @param sha   The 40-character hex SHA-1 hash of the entry's object.
 * 
 * @note The tree must be sorted before serialization. This function just appends
 *       to the internal array sequentially.
 **/
void tree_add_entry(Tree *t, const char *mode, const char *path, const char *sha){
    if (t->capacity == t->count){
        t->capacity = t->capacity ? t->capacity * 2 : 8;
        t->entries = safe_realloc(t->entries, t->capacity * sizeof(TreeLeaf));
    }

    TreeLeaf *e = &t->entries[t->count++];

    size_t mode_len = strlen(mode);
    if (mode_len == 5){
        e->mode[0] = '0';
        memcpy(e->mode + 1, mode, 5);
        e->mode[6] = '\0';
    } else {
        size_t n = mode_len < 6 ? mode_len : 6;
        memcpy(e->mode, mode, n);
        e->mode[n] = '\0';
    }

    e->path = safe_strdup(path ? path: "");
    strncpy(e->sha, sha, 40);
    e->sha[40] = '\0';
}

/**
 * tree_entry_type - determines the Git object type based on a file mode
 * 
 * @param mode  The 6-character octal mode string (e.g., "040000").
 **/
const char *tree_entry_type(const char *mode){
    if (mode[0] == '0' && mode[1] == '4') return "tree";
    if (mode[0] == '1' && mode[1] == '0') return "blob";
    if (mode[0] == '1' && mode[1] == '2') return "blob";
    if (mode[0] == '1' && mode[1] == '6') return "commit";
    return NULL;
}

/**
 * parse_one - extracts a single tree entry from a raw binary payload
 * 
 * Scans the raw buffer starting at the given offset to extract the file mode 
 * (space terminated), the file path (null terminated), and the 20-byte binary 
 * SHA-1 hash. Converts the binary SHA-1 to a 40-character hex string and adds 
 * the fully parsed entry to the Tree structure.
 * 
 * @param raw   Pointer to the raw binary tree object payload.
 * @param len   The total length in bytes of the raw payload buffer.
 * @param start The byte offset in the buffer where this specific entry begins.
 * @param tree  Pointer to the Tree struct where the extracted entry will be added.
 * 
 * @return A pointer to the byte immediately following the parsed entry (the start 
 *         of the next entry), or NULL if parsing fails due to malformed data or 
 *         buffer overflows.
 **/
static const unsigned char *parse_one(const unsigned char *raw, size_t len, size_t start, Tree *tree){
    size_t x = start;
    while (x < len && raw[x] != ' '){ x++; }
    if (x >= len) return NULL;

    size_t mode_len = x - start;
    if (mode_len != 5 && mode_len !=6) return NULL;

    char mode[7] = {0};
    memcpy(mode, raw + start, mode_len);

    size_t y = x + 1;
    while (y < len && raw[y] != '\x00'){ y++; };
    if (y >= len) return NULL;
    
    size_t path_len = y - (x + 1);
    char *path = safe_calloc(path_len + 1, 1);
    memcpy(path, raw + x + 1, path_len);

    if (y + 21 > len){
        free(path);
        return NULL;
    }

    char sha[41] = {0};
    for (int i = 0; i < 20; i++){
        snprintf(sha + i * 2, 3, "%02x", raw[ y + 1 + i]);
    }
    tree_add_entry(tree, mode, path, sha);
    free(path);
    return raw + y + 21;
}

/**
 * tree_parse - parses raw binary tree data into a Tree struct
 * 
 * Iterates through the raw Git tree object payload, extracting the mode,
 * path, and 20-byte binary SHA-1 for each entry, converting the SHA-1 back
 * to a 40-character hex string, and adding the entry to a new Tree struct.
 * 
 * @param raw_data Pointer to the raw binary payload of a tree object.
 * @param len      The size in bytes of the raw_data buffer.
 * 
 * @return A pointer to the populated Tree struct, or NULL if parsing fails.
 **/
Tree *tree_parse(const char *raw, size_t len){
    Tree *t = tree_new();
    const unsigned char *base= (const unsigned char *)raw;
    size_t pos = 0;

    while (pos < len){
        const unsigned char *next = parse_one(base, len, pos, t);
        if (!next){
            tree_destroy(t);
            fprintf(stderr, "tree_parse: malformed tree data\n");
            return NULL;
        }
        pos = (size_t)(next - base);
    }
    return t;
}

/**
 * entry_cmp - comparison function for sorting tree entries
 * 
 * Compares two TreeLeaf entries for qsort. Enforces Git's specific 
 * tree sorting rules where a directory (tree) entry is treated as if 
 * its path string ends with a trailing slash ('/') for comparison purposes.
 * 
 * @param a Pointer to the first TreeLeaf (cast to const void *).
 * @param b Pointer to the second TreeLeaf (cast to const void *).
 * 
 * @return An integer less than, equal to, or greater than zero if the first
 *         entry is found, respectively, to be less than, to match, or be 
 *         greater than the second entry.
 * 
 * @note This specific sorting order is strictly required by Git to ensure 
 *       consistent tree object hashes.
 **/
static int entry_cmp(const void *a, const void *b){
    const TreeLeaf *ea = (const TreeLeaf *)a;
    const TreeLeaf *eb = (const TreeLeaf *)b;

    bool a_is_tree = (ea->mode[1] == '4');
    bool b_is_tree = (eb->mode[1] == '4');

    char *ka = safe_malloc(strlen(ea->path) + 2, 1);
    char *kb = safe_malloc(strlen(eb->path) + 2, 1);
    sprintf(ka, a_is_tree ? "%s/" : "%s", ea->path);
    sprintf(kb, b_is_tree ? "%s/" : "%s", eb->path);

    int result = strcmp(ka, kb);
    free(ka);
    free(kb);
    return result;
}

/**
 * tree_serialize - serializes a Tree into the Git raw binary format
 * 
 * Sorts the entries according to Git's tree sorting rules (treating trees as 
 * having a trailing slash), then packs the mode, path, and raw 20-byte SHA-1 
 * into a continuous byte array.
 * 
 * @param tree Pointer to the Tree struct to serialize.
 * @param len  Pointer to a size_t where the resulting byte array length will be stored.
 * 
 * @return A pointer to the newly allocated byte array containing the serialized tree.
 *         The caller is responsible for freeing this memory.
 **/
char *tree_serialize(Tree *t, size_t *out_len){
    qsort(t->entries, t->count, sizeof(TreeLeaf), entry_cmp);

    DynBuf buf;
    dynbuf_init(&buf);

    for (size_t i = 0; i < t->count; i++){
        TreeLeaf *e = &t->entries[i];
        dynbuf_append(&buf, e->mode, strlen(e->mode));
        dynbuf_append(&buf, " ", 1);
        dynbuf_append(&buf, e->path, strlen(e->path));
        dynbuf_append(&buf, "\x00", 1);

        unsigned char sha_bin[20];
        for (int b = 0; b < 20; b++) {
            unsigned int byte;
            sscanf(e->sha + b * 2, "%2x", &byte);
            sha_bin[b] = (unsigned char)byte;
        }

        dynbuf_append(&buf, sha_bin, 20);
    }

    *out_len = buf.len;
    return buf.data;
}

/**
 * ls_tree - recursively or flatly prints the contents of a tree
 * 
 * Resolves the given reference to a tree object, parses it, and prints 
 * its entries to standard output. If the recursive flag is set, it will 
 * recursively traverse into sub-trees, appending directory names to the 
 * display path.
 * 
 * @param repo      Pointer to the active Repository struct.
 * @param ref       The SHA-1 hash or reference resolving to a tree.
 * @param recursive True to traverse into sub-trees, false for a flat list.
 * @param prefix    The base path prefix used for printing (e.g., "", "dir/").
 * 
 * @return True if the tree was successfully traversed and printed, false on error.
 **/
bool ls_tree(Repository *repo, const char *ref, bool recursive, const char *prefix){
    char *sha = object_find(repo, ref, GIT_TREE, true);
    if (!sha) return false;

    Object *obj = object_read(repo, sha);
    free(sha);
    if (!obj || obj->type != GIT_TREE) {
        fprintf(stderr, "ls-tree: not a tree: %s\n", ref);
        object_destroy(obj);
        return false;
    }

    Tree *t= object_to_tree(obj);
    object_destroy(obj);
    if (!t) return false;

    bool ok = true;
    for (size_t i = 0; i < t->count; i++){
        TreeLeaf *e = &t->entries[i];
        const char *type = tree_entry_type(e->mode);
        if (!type) {
            fprintf(stderr, "ls-tree: weird tree leaf mode %s\n", e->mode);
            ok = false;
            continue;
        }
        
        char *full_path = path_join(prefix ? prefix : "", e->path, NULL);
        const char *display_path = (prefix && *prefix) ? full_path : e->path;

        if (recursive && streq(type, "tree")){
            if (!ls_tree(repo, e->sha, true, full_path)) ok = false;
        } else {
            printf("%s %s %s\t%s\n", e->mode, type, e->sha, display_path);    
        }
        free(full_path);
    }

    tree_destroy(t);
    return ok;
}

/**
 * tree_checkout - instantiates the contents of a tree in the filesystem
 * 
 * Recursively traverses a Tree structure, creating directories for nested trees
 * and writing file contents for blobs into the specified destination directory.
 * 
 * @param repo     Pointer to the active Repository struct.
 * @param tree     Pointer to the Tree struct representing the directory state.
 * @param dest_dir The file system path where the tree should be extracted.
 * 
 * @return True if the checkout succeeds entirely, false if an error occurs.
 **/
bool tree_checkout(Repository *repo, Tree *tree, const char *path){
    for (size_t i = 0; i < tree->count; i++) {
        TreeLeaf *e = &tree->entries[i];
        Object *obj = object_read(repo, e->sha);
        if (!obj) {
            fprintf(stderr, "checkout: cannot read object %s\n", e->sha);
            return false;
        }

        char *dest = path_join(path, e->path, NULL);
        bool ok = true;

        if (obj->type == GIT_TREE) {
            if (mkdir(dest, 0755) != 0) {
                fprintf(stderr, "checkout: cannot create directory %s\n", dest);
                ok = false;
            } else {
                Tree *sub = object_to_tree(obj);
                ok = sub && tree_checkout(repo, sub, dest);
                tree_destroy(sub);
            }
        } else if (obj->type == GIT_BLOB) {
            /* TODO: symlinks (mode 12****) */ 
            FILE *f = safe_fopen(dest, "wb");
            fwrite(obj->data, 1, obj->size, f);
            fclose(f);
        } else {
            fprintf(stderr, "checkout: unexpected object type in tree\n");
            ok = false;
        }

        free(dest);
        object_destroy(obj);
        if (!ok) return false;
    }
    return true;
}

/**
 * dirmap_get_or_create - Retrieve or create a bucket for a directory.
 *
 * Searches the map for a bucket matching the given directory path.
 * If none exists, a new bucket is created and initialised.
 *
 * @param m   Pointer to the DirMap.
 * @param dir Directory path ("" for root).
 *
 * @return Pointer to the (possibly new) DirBucket.
 **/
static DirBucket *dirmap_get_or_create(DirMap *m, const char *dir){
    for (size_t i = 0; i < m->count; i++){
        if (streq(m->buckets[i].dir, dir)) return &m->buckets[i];
    }

    if (m->capacity == m->count){
        m->capacity = m->capacity ? m->capacity * 2 : 16;
        m->buckets = safe_realloc(m->buckets, sizeof(DirBucket) * m->capacity);
    }
    DirBucket *b = &m->buckets[m->count++];
    b->dir       = safe_strdup(dir);
    b->items     = 0;
    b->count     = 0;
    b->capacity  = 0;
    return b;
}

/**
 * dirbucket_add - Append a DirItem to a bucket.
 *
 * Grows the bucket's item array if necessary and copies the item.
 *
 * @param b    Pointer to the DirBucket.
 * @param item The DirItem to add (copied by value).
 **/
static void dirbucket_add(DirBucket *b, DirItem item){
    if (b->count == b->capacity){
        b->capacity = b->capacity ? b->capacity * 2 : 16;
        b->items = safe_realloc(b->items, sizeof(DirItem) * b->capacity);
    }
    b->items[b->count++] = item;
}

/**
 * bucket_cmp_len_desc - Comparison function for sorting buckets by depth.
 *
 * Orders buckets from deepest (longest directory string) to root (shortest).
 *
 * @param a Pointer to first DirBucket.
 * @param b Pointer to second DirBucket.
 *
 * @return Negative if a is shallower, positive if deeper, zero if equal.
 **/
static int bucket_cmp_len_desc(const void *a, const void *b){
    size_t la = strlen(((const DirBucket *)a)->dir);
    size_t lb = strlen(((const DirBucket *)b)->dir);
    return (int)lb - (int)la;
}

/**
 * dirname_of - Extract the parent directory from a path.
 *
 * Returns a newly allocated string containing everything before the
 * last slash, or an empty string if there is no slash. The caller
 * must free the result.
 *
 * @param path Input path.
 *
 * @return Newly allocated parent directory string (or "" if none).
 **/
static char *dirname_of(const char *path){
    const char *slash = strrchr(path, '/');
    if (!slash) return safe_strdup("");
    size_t len = (size_t)(slash - path);
    char *out = safe_calloc(len + 1, 1);
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

/**
 * basename_of - Return a pointer to the final component of a path.
 *
 * @param path Input path.
 *
 * @return Pointer to the basename (within the original string).
 **/
static const char *basename_of(const char *path){
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/**
 * tree_from_index - Convert a flat index into a recursive tree object.
 *
 * Builds a directory map from the index, then writes a tree object for every
 * directory (bottom‑up). Returns the SHA‑1 of the root tree.
 *
 * @param repo Repository pointer.
 * @param idx  The GitIndex to convert.
 * @return Newly allocated SHA‑1 hex string of the root tree, or NULL on failure.
 */
char *tree_from_index(Repository *repo, GitIndex *idx){
    DirMap map = {0};
    dirmap_get_or_create(&map, "");

    for (size_t i = 0; i < idx->count; i++){
        char *dir = dirname_of(idx->entries[i].name);

        char *key = safe_strdup(dir);
        while (!streq(key, "")){
            dirmap_get_or_create(&map, key);
            char *parent = dirname_of(key);
            free(key);
            key = parent;
        }
        free(key);

        DirBucket *b = dirmap_get_or_create(&map, dir);
        DirItem item = {0};
        item.is_tree_ref = false;
        strncpy(item.name, basename_of(idx->entries[i].name), sizeof(item.name) -1);
        snprintf(item.mode, sizeof(item.mode), "%02o%04o",
                idx->entries[i].mode_type, idx->entries[i].mode_perms);
        memcpy(item.sha, idx->entries[i].sha, 41);
        dirbucket_add(b, item);

        free(dir);
    }

    qsort(map.buckets, map.count, sizeof(DirBucket), bucket_cmp_len_desc);

    char *root_sha = NULL;
    for (size_t i = 0; i < map.count; i++){
        DirBucket *b = &map.buckets[i];

        char *this_dir = safe_strdup(b->dir);
        Tree *tree = tree_new();
        for (size_t j = 0; j < b->count; j++){
            DirItem *it = &b->items[j];
            const char *mode = it->is_tree_ref ? "040000" : it->mode;
            tree_add_entry(tree, mode, it->name, it->sha);
        }

        Object *tree_obj = tree_to_object(tree);
        char *sha = object_write(tree_obj, repo);
        tree_destroy(tree);
        object_destroy(tree_obj);

        char *parent_dir = dirname_of(this_dir);
        bool is_root = streq(this_dir, "");
        free(this_dir);
        
        if (is_root){
            root_sha = safe_strdup(sha);
        } else {
            DirBucket *parent = dirmap_get_or_create(&map, parent_dir);
            DirItem tree_item = {0};
            tree_item.is_tree_ref = true;
            strncpy(tree_item.name, basename_of(b->dir), sizeof(tree_item.name) -1);
            memcpy(tree_item.sha, sha, 41);
            dirbucket_add(parent, tree_item);
        }
        free(parent_dir);
        free(sha);
    }

    for (size_t i = 0; i < map.count; i++){
        free(map.buckets[i].dir);
        free(map.buckets[i].items);
    }

    free(map.buckets);

    return root_sha;
}
