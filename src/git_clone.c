/* git_clone.c: git driver */

#include "git_functions.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>


int main(int argc, char *argv[]){
    int argind = 1;
    bool status = true;
    if (argc == argind){
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 1 && (streq(argv[1], "-h") || streq(argv[1], "--help"))){
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *command = argv[argind++];

    if (streq(command, "init")){
        status = cmd_init(argc - argind, &argv[argind]);
    } else if (streq(command, "cat-file")){
        status = cmd_cat_file(argc - argind, &argv[argind]);
    } else if (streq(command, "hash-object")){
        status = cmd_hash_object(argc - argind, &argv[argind]);
    } else if (streq(command, "log")){
        status = cmd_log(argc - argind, &argv[argind]);
    } else if (streq(command, "ls-tree")){
        status = cmd_ls_tree(argc - argind, &argv[argind]);
    } else if (streq(command, "checkout")){
        status = cmd_checkout(argc - argind, &argv[argind]);
    } else if (streq(command, "show-ref")){
        status = cmd_show_ref(argc - argind, &argv[argind]);
    } else if (streq(command, "tag")){
        status = cmd_tag(argc - argind, &argv[argind]);
    } else if (streq(command, "rev-parse")){
        status = cmd_rev_parse(argc - argind, &argv[argind]);
    } else {
        fprintf(stderr, "%s: '%s' is not a valid command.\n\n", argv[0], command);
        usage(argv[0]);
        status = false;
    }

    return status ? EXIT_SUCCESS : EXIT_FAILURE;
}