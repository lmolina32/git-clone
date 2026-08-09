/* git_clone.c: git driver */

#include "git_functions.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>


int main(int argc, char *argv[]){
    int argind = 1;
    bool status = true;
    if (argc == argind){
        // TODO: usage message, return exit_failure 
    }
    if (argc > 1 && (streq(argv[1], "-h") || streq(argv[1], "--help"))){
        // TODO: usage message, return exit_success
    }

    const char *command = argv[argind++];

    if (streq(command, "init")){
        status = cmd_init(argc - argind, &argv[argind]);
    } else if (streq(command, "cat-file")){
        status = cmd_cat_file(argc - argind, &argv[argind]);
    } else if (streq(command, "hash-object")){
        status = cmd_hash_object(argc - argind, &argv[argind]);
    }

    return status ? EXIT_SUCCESS : EXIT_FAILURE;
}