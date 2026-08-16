/* objects.h */

#ifndef OBJECTS_H
#define OBJECTS_H

#include "repository.h"
#include "kvlm.h"
#include "tree.h"

#include <stdio.h>

/* Macros */

#define GIT_ANY_TYPE ((object_type)-1)

/* Structures */

typedef enum {
    GIT_BLOB,
    GIT_COMMIT, 
    GIT_TAG,
    GIT_TREE,
} object_type;

typedef struct {
    object_type  type;
    char        *data;
    size_t       size;
} Object;

/* Functions */

const char *object_type_name(object_type type);
bool        object_type_from_name(const char *name, object_type *out);
Object     *object_new(object_type type, char *data, size_t size);
void        object_destroy(Object *obj);
Object     *object_read(Repository *repo, const char *sha);
char       *object_write(Object *obj, Repository *repo);
char       *object_find(Repository *repo, const char *name, object_type type, bool follow);
char       *object_hash(int fd, object_type type, Repository *repo);
Object     *object_from_kvlm(object_type type, KVLM *kvlm);

bool        cat_file(Repository *repo, const char *name, object_type type);
KVLM       *commit_parse(Object *obj);
Object     *commit_from_kvlm(KVLM *kvlm);
Tree       *object_to_tree(Object *obj);
Object     *tree_to_object(Tree *tree);


#endif
