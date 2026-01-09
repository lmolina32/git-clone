/* repository.c: handles repo functions */

#include "repository.h"
#include "ini.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>

/* Forward Declaration of static Functions */

static int handler(void* user, const char* section, const char* name, const char* value);
static char *build_path(Repository *repo, va_list args);

/* Functions */

/**
 * repo_create - Allocates and initializes a Repository object (Constructor).
 *  
 * It sets the worktree and gitdir paths, and unless 'force' is true, 
 * validates that the directory is a valid Git repository by checking 
 * for the existence of the .git directory and verifying the 
 * 'repositoryformatversion' in the config file.
 *
 * @param path  The path to the repository's worktree.
 * @param force If true, skip validation checks (used during initial creation).
 * @return      A pointer to the allocated Repository struct, or NULL if 
 * validation fails (e.g., missing config or unsupported version).
 * @note        The caller is responsible for memory cleanup via repo_destroy().
 */
Repository *repo_create(const char *path, bool force){
    if (!path) { return NULL; }

    Repository *repo = safe_calloc(sizeof(Repository), 1);
    strncpy(repo->worktree, path, strlen(path) + 1);
    char *gitdir = path_join(path, ".git", NULL);
    if (gitdir){
        strncpy(repo->gitdir, gitdir, strlen(gitdir) + 1);
        free(gitdir);
    }

    if (!(force || is_directory(repo->gitdir))){
        fprintf(stderr, "repo_create: Not a Git Repository %s\n", repo->gitdir);
        goto fail;
    }

    char *config_file_path = path_join(repo->gitdir, "config", NULL);

    if (config_file_path && file_exists(config_file_path)){
        repo->config = repo_config_create(config_file_path);
        if (!repo->config){ goto fail; }
    } else if (!force){
       fprintf(stderr, "repo_create: Configuration File is missing\n"); 
       goto fail;
    }

    if (!force && repo->config->repo_format_version != 0){
        fprintf(stderr, "repo_create: Unsupported repositoryformatversion: %d\n", repo->config->repo_format_version);
        goto fail;
    }

    return repo;

    fail:
    repo_destroy(repo);
    return NULL;
}

/**
 * repo_init - Creates a new repository at the specified path.
 *
 * This function initializes the directory structure for a new repository, 
 * including .git/objects, .git/refs/heads, etc., and writes the default 
 * configuration and HEAD files.
 *
 * @param path The filesystem path where the repository should be created.
 * @return A pointer to the newly created Repository object, or NULL on failure.
 * @note The caller is responsible for destroying the returned Repository using repo_destroy().
 */
Repository *repo_init(const char *path){
    if (!path) { return NULL; }
    Repository *repo = repo_create(path, true);

    if (!repo) { return NULL; }

    struct stat sb;
    if (stat(repo->worktree, &sb) == 0){
        if (!S_ISDIR(sb.st_mode)){
            fprintf(stderr, "repo_init: %s is not a directory\n", repo->worktree);
            goto fail;
        } 
        if (stat(repo->gitdir, &sb) == 0 && !is_directory_empty(repo->gitdir)){
            fprintf(stderr, "repo_init: %s directory is not empty\n", repo->gitdir);
            goto fail;
        }
    } else {
        if(!mkdir_p(repo->worktree, (mode_t)0755)){
            fprintf(stderr, "repo_init: cannot create directory %s\n", repo->worktree);
            goto fail;
        }
    }

    char *s = repo_dir(repo, true, "branches", NULL);
    assert(s != NULL);
    free(s);
    s = repo_dir(repo, true, "objects", NULL);
    assert(s != NULL);
    free(s);
    s = repo_dir(repo, true, "refs", "tags", NULL);
    assert(s != NULL);
    free(s);
    s = repo_dir(repo, true, "refs", "heads", NULL);
    assert(s != NULL);
    free(s);

    s = repo_file(repo, false, "description", NULL);
    FILE *f = safe_fopen(s, "w");
    fprintf(f, "Unnamed repository; edit this file 'description' to name the repository.\n");
    fclose(f);
    free(s);

    s = repo_file(repo, false, "HEAD", NULL);
    f = safe_fopen(s, "w");
    fprintf(f, "ref: refs/heads/master\n");
    fclose(f);
    free(s);

    s = repo_file(repo, false, "config", NULL);
    f = safe_fopen(s, "w");
    fprintf(f, "[core]\n");
    fprintf(f, "repositoryformatversion = 0\n");
    fprintf(f, "filemode = false\n");
    fprintf(f, "bare = false\n");
    fclose(f);
    free(s);

    return repo;

fail:
    repo_destroy(repo);
    return NULL;
}

/**
 * repo_find - Recursively searches for a Git repository starting from the given path.
 *
 * Walks up the directory tree looking for a ".git" directory.
 *
 * @param path The directory to start the search from.
 * @param required If true, the function will terminate the program or log a 
 * fatal error if no repository is found.
 * @return A pointer to the found Repository object, or NULL if not found and not required.
 * @note The caller is responsible for destroying the returned Repository.
 */
Repository *repo_find(const char *path, bool required){
    if (!path) { return NULL; }
    if (required) {}
    return NULL;
}



/**
 * repo_config_create - Loads repository configuration from an INI file.
 *
 * This function parses a git-style INI configuration file at the specified path,
 * allocating and populating a Configuration struct with values for repository
 * format version, filemode, and bare status.
 *
 * @param path The filesystem path to the INI configuration file (e.g., .git/config).
 * @return A pointer to the newly allocated Configuration object, or NULL if the
 * file could not be opened, parsed, or if memory allocation failed.
 * @note The caller is responsible for freeing the returned Configuration pointer 
 * using free() when it is no longer needed.
 */
Configuration *repo_config_create(const char *path){
    Configuration *config = safe_calloc(sizeof(Configuration), 1);

    if (ini_parse(path, handler, config) < 0){
        fprintf(stderr, "repo_config_create: cannot load %s\n", path);
        return NULL;
    }

    return config;
}

/**
 * repo_destory - Frees all memory associated with a Repository object.
 *
 * Closes any open file descriptors, frees internal strings, and finally 
 * frees the Repository struct itself.
 *
 * @param repo The Repository object to deallocate.
 */
void repo_destroy(Repository *repo){
    if (!repo) { return; }
    if (repo->config){
        free(repo->config);
        repo->config = NULL;
    }

    free(repo);
}

/**
 * repo_path - Computes a path relative to the repository's gitdir (.git folder).
 *
 * @param repo The repository context.
 * @param ...   Additional strings to join. The list MUST end with NULL.
 * @note The caller MUST provide NULL as the last argument to signal the end.
 * @return A heap-allocated string containing the full path. 
 * @note The caller MUST free() the returned string when finished.
 */
char *repo_path(Repository *repo, ...){
    if (!repo) { return NULL; }
    va_list args;
    va_start(args, repo);
    char *arg_path = build_path(repo, args);
    va_end(args);
    return arg_path;
}

/**
 * repo_file - Computes a path under gitdir and ensures the parent directory exists.
 *
 * Similar to repo_path, but if 'mkdir' is true, it will attempt to create 
 * the leading directory components if they are missing.
 *
 * @param repo The repository context.
 * @param mkdir If true, create the directory containing the file.
 * @param ...   Additional strings to join. The list MUST end with NULL.
 * @return A heap-allocated string of the full path, or NULL if directories 
 * @note The caller MUST provide NULL as the last argument to signal the end.
 * could not be created.
 * @note The caller MUST free() the returned string when finished.
 */
char *repo_file(Repository *repo, bool mkdir, ...){
    if (!repo) { return NULL; }

    va_list args;
    va_start(args, mkdir);
    char *path = build_path(repo, args);
    va_end(args);

    int ch = '/';
    char *valid_path = path + strlen(repo->gitdir) + 1;
    char *last_dir = strrchr(valid_path, ch);

    if (last_dir){
        *last_dir = '\0';
        char *dir_path = repo_dir(repo, mkdir, valid_path, NULL);
        if (dir_path){
            free(dir_path);
            *last_dir = '/';
            return path;
        }
    } else {
        char *dir_path = repo_dir(repo, mkdir, NULL);
        if (dir_path){
            free(dir_path);
            return path;
        }
    }

    free(path);
    return NULL;
}

/**
 * repo_dir - Computes a path under gitdir and ensures the directory itself exists.
 *
 * @param repo The repository context.
 * @param mkdir If true, create the directory (and parents) if missing.
 * @param ...   Additional strings to join. The list MUST end with NULL.
 * @return A heap-allocated string of the full path, or NULL if path exists 
 * but is not a directory, or if creation failed.
 * @note The caller MUST provide NULL as the last argument to signal the end.
 * @note The caller MUST free() the returned string when finished.
 */
char *repo_dir(Repository *repo, bool mkdir, ...){
    if (!repo) { return NULL; }
    
    va_list args;
    va_start(args, mkdir);
    char *path = build_path(repo, args);
    va_end(args);
    
    struct stat sb;
    if (stat(path, &sb) == 0){
        if (S_ISDIR(sb.st_mode)){ return path; }
        else {
            fprintf(stderr,"repo_dir: Not a directory: %s\n", path);
            goto fail;
        }
    }

    if (mkdir && mkdir_p(path, (mode_t)0755)){ return path; } 

    fail:
    free(path);
    return NULL;
}


/* Static Functions */

/**
 * handler - Callback function for the INI parser to populate a Configuration struct.
 *
 * This function is invoked by the INI parser for every section/name/value triplet 
 * found in the configuration file. It maps specific INI keys (like protocol.version) 
 * to the corresponding fields in the Configuration structure passed via the user pointer.
 *
 * @param user A pointer to the Configuration struct being populated (passed as void*).
 * @param section The name of the current INI section (e.g., "core", "user").
 * @param name The name of the configuration key.
 * @param value The value associated with the key as a string.
 * @return Returns 1 on a successful match and update, or 0 if the section/name 
 * is unknown or an error occurred.
 */
static int handler(void* user, const char* section, const char* name, const char* value){
    Configuration *pconfig = (Configuration*) user;

    #define MATCH(s, n) streq(section, s) && streq(name, n)
    if (MATCH("core", "repositoryformatversion")){
        pconfig->repo_format_version = atoi(value);
    } else if (MATCH("core", "filemode")){
        pconfig->filemode = streq(value, "true");
    } else if (MATCH("core", "bare")){
        pconfig->bare = streq(value, "true");
    }

    return 1;
}

/**
 * build_path - Variadic helper to construct a filesystem path from multiple components.
 *
 * This function takes a variable list of string segments and joins them using the 
 * system's path separator. It iteratively allocates memory for the growing path, 
 * ensuring each segment is properly appended.
 *
 * @param repo The repository context.
 * @param args A va_list of const char* path segments. The list must be NULL-terminated.
 * @return A dynamically allocated string containing the full path, or NULL if the 
 * initial segment is missing.
 * @note The caller is responsible for freeing the returned string. This function 
 * consumes the va_list; the caller must handle va_start and va_end.
 */
static char *build_path(Repository *repo, va_list args){
    char *path = NULL;
    char *full_path = NULL;
    const char *s = va_arg(args, const char *);
    if (!s) {return safe_strdup(repo->gitdir); }
    path = path_join(repo->gitdir, s, NULL);

    while ((s = va_arg(args, const char *))){
        full_path = path_join(path, s, NULL);
        free(path);
        path = full_path;
    }
    return full_path != NULL ? full_path : path;
}