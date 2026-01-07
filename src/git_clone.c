/* git_clone.c: git driver */

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include 


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
        status = cmd_init(argind - argc, argv[argind]);
    }


    return status;
}