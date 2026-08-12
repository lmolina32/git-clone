/* git_functions: functions for main git driver */

#include "git_functions.h"
#include "repository.h"
#include "objects.h"
#include "kvlm.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * usage - prints the options for the program
 * 
 * @param program The program name
 **/
void usage(const char *program) {
    fprintf(stderr, "Usage: %s <command> [<args>]\n\n", program);

    fprintf(stderr, "Repository:\n");
    fprintf(stderr, "   init [directory]                 Create an empty Git repository.\n");

    fprintf(stderr, "\nObjects (plumbing):\n");
    fprintf(stderr, "   hash-object [-w] [-t TYPE] FILE  Compute object hash, optionally write it to the store.\n");
    fprintf(stderr, "   cat-file TYPE OBJECT              Print the contents of a repository object.\n");

    fprintf(stderr, "\nHistory:\n");
    fprintf(stderr, "   log [commit]                     Display commit history as Graphviz output.\n");

    fprintf(stderr, "\nGeneral Options:\n");
    fprintf(stderr, "   -h or --help                       Print this help message.\n");
}

/**
 * cmd_init - Initialize a new repository.
 *
 * This function implements the `init` command for the application.
 * It parses command-line arguments, creates the repository directory
 * structure, and initializes all required metadata files.
 *
 * @param arg_count  Number of command-line arguments.
 * @param args       Array of argument strings.
 *
 * @return true if the repository was successfully initialized,
 *         false otherwise.
 */
bool cmd_init(int arg_count, char *argv[]){
    if (arg_count > 1){ 
        fprintf(stderr, "usage: ./git_clone init [<directory>]\n"); 
        return false; 
    }

    Repository *repo;
    if (arg_count){
        repo = repo_init(argv[0]);
    } else {
        repo = repo_init(".");
    }

    if (repo){
        repo_destroy(repo);
        return true;
    }

    return false;
}

/**
 * cmd_cat_file - prints raw contents of an object to stdout 
 * 
 * This function implements the 'cat-file' command for the application. 
 * It parses the command-line arguments to determine the expected object type 
 * and the object identifier. It then locates the current Git repository, 
 * retrieves the specified object, and prints its uncompressed contents to 
 * standard output.
 * 
 * @param arg_count  Number of command-line arguments.
 * @param args       Array of argument strings (expected: <type> <object>).
 *
 * @return true if the object was successfully found and printed,
 *         false on invalid arguments, unknown type, or read failure.
 **/
bool cmd_cat_file(int arg_count, char *args[]){
    if (arg_count != 2){
        fprintf(stderr, "usage ./git_clone cat-file <type> <object>\n");
        return false;
    }

    object_type type;
    if (!object_type_from_name(args[0], &type)){
        fprintf(stderr, "cat-file: unknown type '%s'\n", args[0]);
        return false;
    }

    Repository *repo = repo_find(".", true);
    bool ok = cat_file(repo, args[1], type);
    repo_destroy(repo);
    return ok;
}

/**
 * cmd_hash_object - computes object ID and optionally creates an object from a file
 * 
 * This function implements the 'hash-object' command for the application. 
 * It parses command-line arguments to determine the target file, the object 
 * type (-t), and whether to write the resulting object to the repository 
 * database (-w). It then computes the SHA-1 hash of the file's contents, 
 * prints the hash to stdout, and optionally saves the object.
 * 
 * @param arg_count  Number of command-line arguments.
 * @param args       Array of argument strings.
 *
 * @return true if the object hash was successfully computed and printed,
 *         false otherwise.
 **/
bool cmd_hash_object(int arg_count, char *args[]){
    object_type type = GIT_BLOB;
    bool write = false;
    char *path = NULL;

    for (int i = 0; i < arg_count; i++){
        if (streq(args[i], "-w")){
            write = true;
        } else if (streq(args[i], "-t")){
            if (i + 1 >= arg_count){
                fprintf(stderr, "usage: ./git_clone hash-object [-w] [-t <type>] <file>\n");
                return false;
            }
            if (!object_type_from_name(args[++i], &type)){
                fprintf(stderr, "hash-object: unknown type '%s'\n", args[0]);
                return false;
            }
        } else {
            path = args[i];
        }
    }

    if (!path){
        fprintf(stderr, "usage: ./git_clone hash-object [-w] [-t <type>] <file>\n");
        return false;
    }

    Repository *repo = write ? repo_find(".", true) : NULL;
    int fd = open(path, O_RDONLY);
    if (fd < 0){ 
        fprintf(stderr, "hash-object: cannot open %s\n", path);
        return false;
    }

    char *sha1 = object_hash(fd, type, repo);
    close(fd);
    if (repo) { repo_destroy(repo); }
    if (!sha1){ return false; }

    printf("%s\n", sha1);
    free(sha1);
    return true;
}

/**
 * cmd_log - executes the 'log' command to generate a commit history graph
 * 
 * Resolves the starting commit (defaulting to "HEAD" if no arguments are provided) 
 * and outputs the commit history in Graphviz DOT format to standard output. 
 * Initializes a StringSet to keep track of visited commits in order to handle 
 * merge commits and prevent infinite loops during traversal.
 * 
 * @param arg_count  The number of arguments passed to the log command.
 * @param args       Array of string arguments (expects at most one: the commit/ref).
 *
 * @return true if the graph was successfully generated, false on invalid usage, 
 *         if the repository cannot be found, or if the commit fails to resolve.
 **/
bool cmd_log(int arg_count, char *args[]){
    if (arg_count > 1){
        fprintf(stderr, "./git_clone log [<commit>]\n");
        return false;
    }
    char *commit = arg_count ? args[0] : "HEAD";

    Repository *repo = repo_find(".", true);
    if (!repo) return false;

    char *sha = object_find(repo, commit, GIT_COMMIT, true);
    if (!sha) {
        fprintf(stderr, "log: cannot resolve '%s'\n", commit);
        repo_destroy(repo);
        return false;
    }

    printf("digraph gitlog{\n");
    printf("  node[shape=rect]\n");

    StringSet set;
    string_set_init(&set);
    bool ok = log_graphviz(repo, sha, &set);
    string_set_destroy(&set);

    printf("}\n");

    free(sha);
    repo_destroy(repo);
    return ok;
}