/* repository.h */

#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <stdio.h>
#include <stdbool.h>

/* Macros */

#define MAX_PATH 4096 

/* Structures */

typedef struct {
   int repo_format_version;
   bool filemode;
   bool bare;
} Configuration;

typedef struct {
    char worktree[MAX_PATH];
    char gitdir[MAX_PATH];
    Configuration *config; 
} Repository;

/* Functions */

Repository    *repo_create(const char *path, bool force);
Repository    *repo_init(const char *path);
Repository    *repo_find(const char *path, bool required);
Configuration *repo_config_create(const char *path);
void           repo_destroy(Repository *repo);
char          *repo_path(Repository *repo, ...);
char          *repo_file(Repository *repo, bool mkdir, ...);
char          *repo_dir(Repository *repo, bool mkdir, ...);

#endif
