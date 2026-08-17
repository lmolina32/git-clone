/* tree.h */

#ifndef TREE_H
#define TREE_H

#include "repository.h"

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

/* Functions */

Tree       *tree_new();
void        tree_destroy(Tree *t);
void        tree_add_entry(Tree *t, const char *mode, const char *path, const char *sha);
Tree       *tree_parse(const char *raw, size_t len);
char       *tree_serialize(Tree *t, size_t *out_len);
const char *tree_entry_type(const char *mode);
bool        ls_tree(Repository *repo, const char *ref, bool recursive, const char *prefix);
bool        tree_checkout(Repository *repo, Tree *tree, const char *path);

#endif