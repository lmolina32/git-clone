/* commit.h */

#ifndef COMMIT_H
#define COMMIT_H

#include "repository.h"

#include <time.h>

/* Functions */

char *commit_create(Repository *repo, const char *tree_sha, const char *parent_sha, 
                    const char *author, time_t timestamp, const char *message);

#endif