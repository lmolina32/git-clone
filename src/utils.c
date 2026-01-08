/* utils.c: utility functions used by git clone */

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        return true;
    }
    return false;
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
