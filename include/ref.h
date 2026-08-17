/* ref.h */

#ifndef REF_H
#define REF_H

#include "repository.h"

typedef struct RefNode RefNode;

struct RefNode{
    char    *name;
    bool     is_leaf;
    char    *sha;
    RefNode *children;
    size_t   child_count;
    size_t   child_capacity;
};

char    *ref_resolve(Repository *repo, const char *ref);
RefNode *ref_list(Repository *repo, const char *dir_path);
void    ref_node_destroy(RefNode *node);
void    ref_node_free_contents(RefNode *node);
void    show_ref(RefNode *node, bool with_hash, const char *prefix);
bool    ref_create(Repository *repo, const char *ref_name, const char *sha);
bool    tag_create(Repository *repo, const char *name, const char *ref, bool create_tag_object);

#endif