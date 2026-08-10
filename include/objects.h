/* objects.h */

#ifndef OBJECTS_H
#define OBJECTS_H

#include <stdio.h>
#include <repository.h>

/* Structures */

typedef enum {
    GIT_BLOB,
    GIT_COMMIT, 
    GIT_TAG,
    GIT_TREE,
} object_type;

typedef struct {
    object_type type;
    char *data;
    size_t size;
} Object;

/* Functions */

const char *object_type_name(object_type type);
bool        object_type_from_name(const char *name, object_type *out);

Object *object_new(object_type type, char *data, size_t size);
void    object_destroy(Object *obj);

Object *object_read(Repository *repo, char *sha);
char   *object_write(Object *obj, Repository *repo);
char   *object_find(Repository *repo, const char *name, object_type type, bool follow);
char   *object_hash(int fd, object_type type, Repository *repo);

bool   cat_file(Repository *repo, const char *name, object_type type);

#endif
