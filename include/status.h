/* status.h */

#ifndef STATUS_H
#define STATUS_H

#include "repository.h"
#include "index.h"
#include "utils.h"

/* Structures */

typedef struct {
    char *path;
    char  sha[41];
} PathEntry;

typedef struct {
    PathEntry *entries;
    size_t count;
    size_t capacity;
} PathMap;

/* Functions */

void status_branch(Repository *repo);
void status_head_index(Repository *repo, GitIndex *idx);
void status_index_worktree(Repository *repo, GitIndex *idx);
void pathmap_set(PathMap *m, const char *path, const char *sha);
PathEntry *pathmap_find(PathMap *m, const char *path);
void pathmap_remove(PathMap *m, const char *path);
void pathmap_destroy(PathMap *m);
bool tree_to_dict(Repository *repo, const char *ref, const char *prefix, PathMap *out);
char *branch_get_active(Repository *repo);
void walk_tree(Repository *repo, const char *dir, StrList *out);


#endif