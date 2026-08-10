/* git_functions */

#ifndef GIT_FUNCTIONS 
#define GIT_FUNCTIONS

#include <stdio.h>

/* Functions */

bool cmd_init(int arg_count, char *args[]);
bool cmd_cat_file(int arg_count, char *args[]);
bool cmd_hash_object(int arg_count, char *args[]);

#endif
