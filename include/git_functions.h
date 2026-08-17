/* git_functions */

#ifndef GIT_FUNCTIONS_H 
#define GIT_FUNCTIONS_H

#include <stdio.h>

/* Functions */

void usage(const char *program);
bool cmd_init(int arg_count, char *args[]);
bool cmd_cat_file(int arg_count, char *args[]);
bool cmd_hash_object(int arg_count, char *args[]);
bool cmd_log(int arg_count, char *args[]);
bool cmd_ls_tree(int arg_count, char *args[]);
bool cmd_checkout(int arg_count, char *args[]);
bool cmd_show_ref(int arg_count, char *args[]);
bool cmd_tag(int arg_count, char *args[]);
bool cmd_rev_parse(int arg_count, char *args[]);
bool cmd_ls_files(int arg_count, char *args[]);
bool cmd_check_ignore(int arg_count, char *args[]);
bool cmd_status(int arg_count, char *args[]);
bool cmd_rm(int arg_count, char *args[]);
bool cmd_add(int arg_count, char *args[]);
bool cmd_commit(int arg_count, char *args[]);

#endif
