/* objects.c: object functions for git */

#include "objects.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

/* Functions */

Object *object_read(Repository *repo, char *sha){
    if (!repo || !sha || strlen(sha) < 3) return NULL;

    char prefix[3] = {sha[0], sha[1], '\0'};
    char *path = repo_file(repo, false, "Objects", prefix, sha + 2, NULL);

    if (!file_exists(path)){
        return NULL;
    }
}