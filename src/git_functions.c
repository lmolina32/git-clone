/* git_functions: functions for main git driver */

#include "git_functions.h"
#include "repository.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/**
 * cmd_init - Initialize a new repository.
 *
 * This function implements the `init` command for the application.
 * It parses command-line arguments, creates the repository directory
 * structure, and initializes all required metadata files.
 *
 * @param arg_count  Number of command-line arguments.
 * @param args       Array of argument strings.
 *
 * @return true if the repository was successfully initialized,
 *         false otherwise.
 */
bool cmd_init(int arg_count, char *argv[]){
    if (arg_count > 1){ 
        fprintf(stderr, "usage: git init [<directory>]\n"); 
        return false; 
    }

    Repository *repo;
    if (arg_count){
        repo = repo_init(argv[0]);
    } else {
        repo = repo_init(".");
    }

    if (repo){
        repo_destroy(repo);
        return true;
    }

    return false;
}