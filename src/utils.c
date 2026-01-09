/* utils.c: utility functions used by git clone */

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

/* Functions */

/**
 * str_join Concatenates two strings into a new heap-allocated buffer.
 * 
 * @param s1 The first string (prefix).
 * @param s2 The second string (suffix).
 * @param ...   Additional strings to join. The list MUST end with NULL.
 * @return A pointer to the new concatenated string, or NULL if memory fails.
 * @note The caller MUST provide NULL as the last argument to signal the end.
 * @note The caller is responsible for calling free() on the returned pointer.
 */
char *path_join(const char *s1, ...){
    if (!s1) { return NULL; }

    int total_len = strlen(s1);
    int str_count = 1;

    // find length to malloc 
    va_list args;
    va_start(args, s1);
    const char *s = NULL;
    while ((s = va_arg(args, const char *))){
        total_len += strlen(s);
        str_count++;
    }
    va_end(args);

    char *str_joined = safe_calloc(sizeof(char), total_len + str_count);

    // construct string 
    size_t s_len = strlen(s1);
    size_t offset = s_len;
    memcpy(str_joined, s1, s_len);
    
    va_start(args, s1);
    while ((s = va_arg(args, const char *))){
        s_len = strlen(s);
        if (s_len == 0) {continue;}
        memcpy(str_joined + offset++, "/", 1);
        memcpy(str_joined + offset, s, s_len);
        offset += s_len;
    }
    va_end(args);

    return str_joined;
}

/**
 * is_directory - Checks if the provided path points to a directory.
 *
 * This function examines the file system at the given path to determine
 * if it exists and is a directory.
 *
 * @param s1 The string path to be checked.
 * @return Returns true if path  is a directory, or false if the path is 
 * invalid or not a directory. 
 */
bool is_directory(const char *path){
    if (!path) { return false; }

    struct stat sb;
    return stat(path, &sb) == 0 && S_ISDIR(sb.st_mode);
}

/**
 * file_exists - Checks for the existence of a file or directory.
 *
 * This function uses the system's stat utility to determine if a given path 
 * exists on the filesystem. It does not distinguish between regular files, 
 * directories, or other special file types.
 *
 * @param path The filesystem path to check.
 * @return True if the path exists and is accessible, false otherwise.
 */
bool file_exists(const char *path){
    if (!path) return false;
    return access(path, F_OK) == 0;
}


/**
 * mkdir_p - Recursively creates a directory and any missing parent directories.
 *
 * This function traverses the provided path and ensures that every directory 
 * level exists. If a directory does not exist, it is created with the 
 * specified mode.
 *
 * @param path The filesystem path to create.
 * @param mode The file mode (permissions) to use for newly created directories.
 * @return True if the directory path was successfully created or already exists, 
 * false if any directory creation failed.
 */
bool mkdir_p(const char *path, mode_t mode){
    if (!path || *path == '\0') { return false; }

    char *full_path = safe_strdup(path);
    char *subpath = full_path;
    struct stat sb;
    bool status = true; 

    if (subpath[0] == '/') { subpath++; }

    while (*subpath != '\0'){
        while (*subpath != '/' && *subpath != '\0') { subpath++; }
        char next_char = *subpath;
        *subpath = '\0'; 

        if (stat(full_path, &sb) != 0){
            if (mkdir(full_path, mode) == -1){ 
                fprintf(stderr, "mkdir_p: failed to make directory\n");
                status = false; 
                break; 
            }
        } else if (!S_ISDIR(sb.st_mode)){
            fprintf(stderr, "mkdir_p: Path given exists but is not a directory\n");
            status = false; 
            break; 
        } 
        if (next_char == '\0') { break; }

        *subpath = next_char;
        subpath++;
    }

    free(full_path);
    return status; 
}

/**
 * is_directory_empty - Determines if a directory contains any files or subdirectories.
 *
 * This function opens the directory at the specified path and iterates through its 
 * entries. It ignores the standard "." and ".." entries. If any other entry is 
 * found, the directory is considered non-empty.
 *
 * @param path The filesystem path to the directory to be checked.
 * @return True if the directory exists and contains no files or subdirectories, 
 * false if it contains entries or if the path could not be opened.
 */
bool is_directory_empty(const char *path){
    if (!path) { return false; }

    bool empty = true;
    DIR *d;

    if ((d = opendir(path)) == NULL){
        fprintf(stderr, "is_directory_empty: cannot open %s\n", path);
        return false;
    }

    for (struct dirent *e = readdir(d); e; e = readdir(d)){
        if(streq(e->d_name, ".") || streq(e->d_name, "..")){ continue; }
        empty = false;
        break;
    }

    closedir(d);
    return empty;
}

/**
 * remove_directory - Removes a directory from the filesystem.
 *
 * This function attempts to delete the directory at the specified path. 
 * Note: Standard implementations using rmdir() will only succeed if the 
 * directory is empty. If the directory contains files or subdirectories, 
 * the operation will fail.
 *
 * @param path The filesystem path of the directory to be removed.
 * @return True if the directory was successfully removed, false if the 
 * path does not exist, is not a directory, or is not empty.
 */
bool remove_directory(const char *path){
    bool status = true;
    DIR *d;

    if ((d = opendir(path)) == NULL){
        fprintf(stderr, "remove_directory: cannot open %s\n", path);
        return false;
    }

    for (struct dirent *e = readdir(d); e; e = readdir(d)){
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0){
            continue;
        }
        
        char sub_path[BUFSIZ];
        if ((ssize_t)snprintf(sub_path, sizeof(sub_path),  "%s/%s", path, e->d_name) >= (ssize_t)sizeof(sub_path)){
            fprintf(stderr, "remove_directory: path %s/%s is too long\n", path, e->d_name);
            status = false;
            continue;
        }

        struct stat s;
        if (stat(sub_path, &s) < 0){
            status = false;
            continue;
        }

        if (S_ISREG(s.st_mode)){
            if(unlink(sub_path) < 0){
                fprintf(stderr, "remove_directory: unable to remove %s: %s\n", sub_path, strerror(errno));
                status = false;
            }
        } else if (S_ISDIR(s.st_mode)){
            if (!remove_directory(sub_path)){ status = false; } 
        }
    }

    closedir(d);

    if (rmdir(path) < 0){
        fprintf(stderr, "remove_directory: unable to remove directory %s, %s\n", path, strerror(errno));
        return false;
    }

    return status;
}