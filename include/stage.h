/* stage.h */

#ifndef STAGE_H
#define STAGE_H

#include "repository.h"

/* Functions */

bool stage_remove(Repository *repo, const char *paths[], size_t path_count, bool delete_files, bool skip_missing);
bool stage_add(Repository *repo, const char *paths[], size_t path_count);

#endif