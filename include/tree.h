/* tree.h */

#ifndef TREE_H
#define TREE_H

#include "repository.h"
#include "index.h"

#include <stddef.h>

/* Structures */

typedef struct {
    char  mode[7];
    char *path;
    char  sha[41];
} TreeLeaf;

typedef struct {
    TreeLeaf *entries;
    size_t    count;
    size_t    capacity;
} Tree;

typedef struct {
    bool is_tree_ref;
    char name[256];
    char mode[16];
    char sha[41];
} DirItem;

typedef struct {
    char    *dir;
    DirItem *items;
    size_t   count;
    size_t   capacity;
} DirBucket;

typedef struct {
    DirBucket *buckets;
    size_t     count;
    size_t     capacity;
} DirMap;

/* Functions */

Tree       *tree_new();
void        tree_destroy(Tree *t);
void        tree_add_entry(Tree *t, const char *mode, const char *path, const char *sha);
Tree       *tree_parse(const char *raw, size_t len);
char       *tree_serialize(Tree *t, size_t *out_len);
const char *tree_entry_type(const char *mode);
bool        ls_tree(Repository *repo, const char *ref, bool recursive, const char *prefix);
bool        tree_checkout(Repository *repo, Tree *tree, const char *path);
char       *tree_from_index(Repository *repo, GitIndex *idx);

#endif
