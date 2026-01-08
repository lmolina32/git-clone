/* repository.h */

#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <stdio.h>
#include <stdbool.h>

#define MAX_PATH 4096 

typedef struct {
    char worktree[MAX_PATH];
    char gitdir[MAX_PATH];
    // TODO add conf -> don't know yet what type to create 
} Repository;

Repository *repo_create(const char *path, bool force);
Repository *repo_init(const char *path);
Repository *repo_find(const char *path, bool required);
void        repo_destory(Repository *repo);
char       *repo_path(Repository *repo, const char *path);
char       *repo_file(Repository *repo, const char *path, bool mkdir);
char       *repo_dir(Repository *repo, const char *path, bool mkdir);
char       *repo_default_config();

#endif
