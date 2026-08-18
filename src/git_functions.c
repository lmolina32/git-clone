/* git_functions: functions for main git driver */

#include "git_functions.h"
#include "repository.h"
#include "objects.h"
#include "kvlm.h"
#include "ref.h"
#include "index.h"
#include "gitignore.h"
#include "status.h"
#include "stage.h"
#include "config.h"
#include "commit.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

/**
 * usage - prints the options for the program
 * 
 * @param program The program name
 **/
void usage(const char *program) {
    fprintf(stderr, "Usage: %s <command> [<args>]\n\n", program);

    fprintf(stderr, "Repository:\n");
    fprintf(stderr, "   init [directory]                    Create an empty Git repository.\n");

    fprintf(stderr, "\nObjects (plumbing):\n");
    fprintf(stderr, "   hash-object [-w] [-t TYPE] FILE     Compute object hash, optionally write it to the store.\n");
    fprintf(stderr, "   cat-file TYPE OBJECT                Print the contents of a repository object.\n");

    fprintf(stderr, "\nHistory:\n");
    fprintf(stderr, "   log [commit]                        Display commit history as Graphviz output.\n");
    fprintf(stderr, "   ls-tree [-r] TREE                   Pretty-print a tree object.\n");
    fprintf(stderr, "   checkout COMMIT PATH                Checkout a commit into an empty directory.\n");

    fprintf(stderr, "\nRefs, tags, and revisions:\n");
    fprintf(stderr, "   show-ref                            List all references\n");
    fprintf(stderr, "   tag [-a] [name] [object]            List tags, or create a new one.\n");
    fprintf(stderr, "   rev-parse [--git-type TYPE] NAME    Resolve a name to an object hash\n");

    fprintf(stderr, "\nStaging area:\n");
    fprintf(stderr, "   ls-files [--verbose]                List files in the staging area.\n");
    fprintf(stderr, "   check-ignore PATH ...               Check paths against ignore rules.\n");
    fprintf(stderr, "   status                              Show the workign tree status.\n");
    fprintf(stderr, "   add PATH ...                        Add files contents to the index\n");
    fprintf(stderr, "   rm PATH ...                         Remove files from the working tree and index.\n");
    fprintf(stderr, "   commit -m MESSAGE                   Record staged changes as a new commit\n");

    fprintf(stderr, "\nGeneral Options:\n");
    fprintf(stderr, "   -h or --help                        Print this help message.\n");
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

/**
 * cmd_ls_tree - handles the execution of the "ls-tree" command
 * 
 * Parses command line arguments to determine if the traversal should be 
 * recursive (-r) and identifies the target tree reference. Locates the 
 * repository and delegates to the core ls_tree() function to print the contents.
 * 
 * @param arg_count The number of arguments passed to the command.
 * @param args      Array of string arguments (e.g., ["-r", "HEAD"]).
 * 
 * @return True on successful execution; false on invalid usage, missing repo,
 *         or if tree traversal fails.
 **/
bool cmd_ls_tree(int arg_count, char *args[]){
    bool recursive = false;
    char *ref= NULL;

    for (int i = 0; i < arg_count; i++){
        if (streq(args[i], "-r")){
            recursive = true;
        } else {
            ref = args[i];
        }
    }

    if (!ref){
        fprintf(stderr, "usage: ./git_clone ls-tree [r] <TREE>\n");
        return false;
    }

    Repository *repo = repo_find(".", true);
    if (!repo) return false;
    bool ok = ls_tree(repo, ref, recursive, "");
    repo_destroy(repo);
    return ok;
}

/**
 * cmd_checkout - handles the execution of the "checkout" command
 * 
 * Resolves the provided reference (commit or tree), verifies the target 
 * directory is safe to use (empty or non-existent), creates it if necessary, 
 * and populates the filesystem with the tree's contents. 
 * 
 * @param arg_count The number of arguments passed to the command.
 * @param args      Array of string arguments: commit/tree ref, and destination path.
 * 
 * @return True if the checkout completes successfully; false on invalid arguments,
 *         missing objects, unsafe destination, or filesystem errors.
 **/
bool cmd_checkout(int arg_count, char *args[]){
    if (arg_count != 2){
        fprintf(stderr, "usage: git checkout <commit> <path>\n");
        return false;
    } 

    const char *commit_ref = args[0];
    const char *dest_path = args[1];

    Repository *repo = repo_find(".", true);
    if (!repo) return false;

    char *sha = object_find(repo, commit_ref, GIT_COMMIT, true);
    Object *obj = sha ? object_read(repo, sha) : NULL;
    free(sha);

    if (!obj){
        fprintf(stderr, "checkout: cannot resolve %s\n", commit_ref);
        repo_destroy(repo);
        return false;
    }

    /* if commit follow to its tree */
    if (obj->type == GIT_COMMIT){
        KVLM *kvlm = commit_parse(obj);
        const char *tree_sha = kvlm_get(kvlm, "tree");
        if (!tree_sha){
            fprintf(stderr, "checkout: commit has no tree\n");
            kvlm_destroy(kvlm);
            object_destroy(obj);
            repo_destroy(repo);
            return false;
        }
        Object *tree_obj = object_read(repo, tree_sha);
        kvlm_destroy(kvlm);
        object_destroy(obj);
        obj = tree_obj;
    }

    if (!obj || obj->type != GIT_TREE) {
        fprintf(stderr, "checkout: %s is not a tree or commit\n", commit_ref);
        object_destroy(obj);
        repo_destroy(repo);
        return false;
    }

    /* verify destination is an empty directory, or doesn't exist yet */
    struct stat sb;
    if (stat(dest_path, &sb) == 0){
        if (!S_ISDIR(sb.st_mode)){
            fprintf(stderr, "checkoout: not a directory: %s\n", dest_path);
            object_destroy(obj);
            repo_destroy(repo);
            return false;
        }

        if (!is_directory_empty(dest_path)){
            fprintf(stderr, "checkout: not empty: %s\n", dest_path);
            object_destroy(obj);
            repo_destroy(repo);
            return false;
        }
    } else if (!mkdir_p(dest_path, 0755)){
        fprintf(stderr, "checkout: cannot create directory %s\n", dest_path);
        object_destroy(obj);
        repo_destroy(repo);
        return false;
    }

    char *real_dst = realpath(dest_path, NULL);
    Tree *tree = object_to_tree(obj);
    bool ok = tree && tree_checkout(repo, tree, real_dst ? real_dst : dest_path);

    free(real_dst);
    tree_destroy(tree);
    object_destroy(obj);
    repo_destroy(repo);
    return ok;
}

/**
 * cmd_show_ref - handles the 'show-ref' command to list repository references
 * 
 * Validates command arguments, locates the active Git repository, recursively
 * reads all stored references, and outputs them in standard format to stdout.
 * 
 * @param arg_count  Number of command-line arguments provided.
 * @param args       Array of command-line argument strings.
 * 
 * @return true if references were successfully listed, false on invalid usage
 *         or if repository location fails.
 **/
bool cmd_show_ref(int arg_count, char *args[]){
    (void)arg_count; (void)args;
    if (arg_count != 0){
        fprintf(stderr, "usage: ./git_clone show-ref\n");
        return false;
    }

    Repository *repo = repo_find(".", true);
    if (repo){
        RefNode *refs = ref_list(repo, NULL);
        repo_destroy(repo);
        if (refs){
            show_ref(refs, true, "refs");
            ref_node_destroy(refs);
            return true;
        }
    }
    return false;
}

/**
 * cmd_tag - handles the 'tag' command to create or display tag references
 * 
 * Parses CLI arguments to create either a lightweight or annotated (`-a`) tag 
 * targeting a specified object (defaulting to "HEAD"), or lists existing tags
 * when no tag name is supplied.
 * 
 * @param arg_count  Number of command-line arguments provided.
 * @param args       Array of command-line argument strings.
 * 
 * @return true if tag creation or listing succeeded, false on missing target names,
 *         invalid options, or repository initialization failure.
 **/
bool cmd_tag(int arg_count, char *args[]){
    bool create_tag_object = false;
    char *name = NULL;
    char *object = "HEAD";
    int pos = 0;

    for (int i = 0; i < arg_count; i++){
        if (streq(args[i], "-a")){ create_tag_object = true; }
        else if (pos == 0){ name = args[i]; pos++; }
        else if (pos == 1){ object = args[i]; pos++; }
    }

    if (!name){
        fprintf(stderr, "usage: ./git_clone tag [-a] <name> [<object>]\n");
        return false;
    }

    Repository *repo = repo_find(".", true);
    if (!repo) return false;

    bool ok = false;
    if (name){
        ok = tag_create(repo, name, object, create_tag_object);
    } else {
        RefNode *refs = ref_list(repo, NULL);
        if (refs){
            for (size_t i = 0; i < refs->child_count; i++){
                if (streq(refs->children[i].name, "tags")){
                    show_ref(&refs->children[i], false, "");
                    ok = true;
                }
            }
            ref_node_destroy(refs);
        }
    }
    repo_destroy(repo);
    return ok;
}

/**
 * cmd_rev_parse - handles the 'rev-parse' command to resolve object names to SHAs
 * 
 * Resolves a given reference, branch name, tag, or short SHA string to its full 
 * 40-character hexadecimal SHA-1 hash and prints it to stdout. Accepts an optional 
 * `--git-type` flag to filter resolution by target object type.
 * 
 * @param arg_count  Number of command-line arguments provided.
 * @param args       Array of command-line argument strings.
 * 
 * @return true if parsing and resolution executed successfully, false on invalid
 *         arguments, bad type specifications, or missing repository context.
 **/
bool cmd_rev_parse(int arg_count, char *args[]){
    object_type type = GIT_ANY_TYPE;
    char *name = NULL;
    
    for (int i = 0; i < arg_count; i++){
        if (streq(args[i], "--git-type")){
            if (i + 1 >= arg_count || !object_type_from_name(args[++i], &type)){
                fprintf(stderr, "rev-parse: bad --git-type argument\n");
                return false;
            }
        } else {
            name = args[i];
        }
    }

    if (!name){
        fprintf(stderr, "usage: git_clone rev-parse [--git-type TYPE] <name>\n");
        return false;
    }

    Repository *repo = repo_find(".", true);
    if (!repo) return false;

    char *sha = object_find(repo, name, type, true);
    repo_destroy(repo);

    printf("%s", sha ? sha : "None");
    free(sha);
    return true;
}

/**
 * cmd_ls_files - lists tracked files in the staging area (index)
 *
 * Reads the repository's index file and prints each entry's path.
 * When the --verbose flag is supplied, it also outputs detailed
 * metadata for every file: type, permissions, blob SHA, timestamps,
 * device/inode, owner/group, and flags.
 *
 * @param arg_count  Number of command-line arguments.
 * @param args       Array of argument strings.
 *
 * @return true on success, false if the repository cannot be located.
 **/
bool cmd_ls_files(int arg_count, char *args[]){
    bool verbose = false;
    for (int i = 0; i < arg_count; i++){
        if (streq(args[i], "--verbose")){ verbose = true; }
    }

    Repository *repo = repo_find(".", true);
    if (!repo) return false;

    GitIndex *idx = index_read(repo);
    if (verbose){
        printf("Index file format v%u, containing %zu entries\n", idx->version, idx->count);
    }

    for (size_t i = 0; i < idx->count; i++){
        IndexEntry *e = &idx->entries[i];
        printf("%s\n", e->name);
        if (!verbose) continue;

        const char *type_name = 
            e->mode_type == 0b1000 ? "regular file" : 
            e->mode_type == 0b1010 ? "symlink"      : 
            e->mode_type == 0b1110 ? "git link"     :
                                     "unkown";
        printf("  %s with perms: %o\n", type_name, e->mode_perms);
        printf("  on blob: %s\n", e->sha);

        char cbuf[32]; 
        char mbuf[32];
        time_t ct = (time_t)e->ctime_s;
        time_t mt = (time_t)e->mtime_s;
        strftime(cbuf,  sizeof(cbuf), "%Y-%m-%d %H:%M:%S", localtime(&ct));
        strftime(mbuf,  sizeof(mbuf), "%Y-%m-%d %H:%M:%S", localtime(&mt));
        printf("  created: %s.%u, modified: %s.%u\n", cbuf, e->ctime_ns, mbuf, e->mtime_ns);
        printf("  device: %u, inode: %u\n", e->dev, e->ino);

        struct passwd *pw = getpwuid(e->uid);
        struct group  *gr = getgrgid(e->gid);
        if (pw && gr){
            printf("  user: %s (%u)  group: %s (%u)\n", pw->pw_name, e->uid, gr->gr_name, e->gid);
        } else {
            printf("  user: %u  group: %u\n", e->uid, e->gid);
        }

        printf("  flags: stage=%u assume_valid=%s\n", e->flag_stage, e->flag_assume_valid ? "true" : "false");

    }
    index_destroy(idx);
    repo_destroy(repo);
    return true;
}

/**
 * cmd_check_ignore - prints which of the given paths are ignored
 *
 * Reads all gitignore rules for the repository and checks each
 * provided path. Paths that match an ignore rule are printed to
 * stdout, one per line.
 *
 * @param arg_count  Number of path arguments.
 * @param args       Array of path strings.
 *
 * @return true on success, false on usage error or repository failure.
 **/
bool cmd_check_ignore(int arg_count, char *args[]){
    if (arg_count < 1){
        fprintf(stderr, "usage: git check-ignore <path> ...\n");
        return false;
    }

    Repository *repo = repo_find(".", true);
    if (!repo) return false;
    
    GitIgnore *rules = gitignore_read(repo);
    for (int i = 0; i < arg_count; i++){
        if (check_ignore(rules, args[i])){
            printf("%s\n", args[i]);
        }
    }
    
    gitignore_destroy(rules);
    repo_destroy(repo);
    return true;
}

/**
 * cmd_status - displays the working tree status
 *
 * Shows the current branch, staged changes (HEAD vs index), and
 * unstaged changes (index vs worktree), including untracked files.
 *
 * @param arg_count  Unused; kept for command dispatch signature.
 * @param args       Unused; kept for command dispatch signature.
 *
 * @return true on success, false if the repository cannot be located.
 **/
bool cmd_status(int arg_count, char *args[]){
    (void)arg_count; (void)args;

    Repository *repo = repo_find(".", true);
    if (!repo) return false;

    GitIndex *idx = index_read(repo);
    status_branch(repo);
    status_head_index(repo, idx);
    printf("\n");
    status_index_worktree(repo, idx);
   
    index_destroy(idx);
    repo_destroy(repo);
    return true;
}

/**
 * cmd_rm - Removes files from both the index and the working tree.
 *
 * For each given path, the corresponding index entry is deleted and,
 * unless the file is outside the worktree, the physical file is also
 * unlinked. This operation is destructive and cannot be undone.
 *
 * @param arg_count  Number of path arguments.
 * @param args       Array of path strings.
 *
 * @return true on success, false on usage error or repository failure.
 **/
bool cmd_rm(int arg_count, char *args[]){
    if (arg_count < 1){
        fprintf(stderr, "usage: ./git_clone rm <path> ...\n");
        return false;
    }

    Repository *repo = repo_find(".", true);
    if (!repo) return false;

    bool ok = stage_remove(repo, (const char **)args, (size_t)arg_count, true, false);
    repo_destroy(repo);
    return ok;
}

/**
 * cmd_add - Adds file contents to the index (stages them).
 *
 * For each given path, the file is hashed into a blob object and a new
 * index entry is created, replacing any existing entry for the same path.
 * The file must exist and be inside the worktree.
 *
 * @param arg_count  Number of path arguments.
 * @param args       Array of path strings.
 *
 * @return true on success, false on usage error or repository failure.
 **/
bool cmd_add(int arg_count, char *args[]){
    if (arg_count < 1){
        fprintf(stderr, "usage: ./git_clone add <path> ...\n");
        return false;
    }    

    Repository *repo = repo_find(".", true);
    if (!repo) return false;

    bool ok = stage_add(repo, (const char **)args, (size_t)arg_count);
    repo_destroy(repo);
    return ok;
}

/**
 * cmd_commit - Creates a new commit from the current index.
 *
 * Builds a tree from the index, creates a commit object with the given
 * message, and updates the current branch (or HEAD) to point to the new
 * commit. The author is taken from the user's git configuration.
 *
 * @param arg_count  Number of arguments (must include -m <message>).
 * @param args       Argument vector.
 *
 * @return true on success, false on usage error or repository failure.
 **/
bool cmd_commit(int arg_count, char *args[]){
    const char *message = NULL;
    for (int i = 0; i < arg_count; i++){
        if (streq(args[i], "-m") && i + 1 < arg_count){
            message = args[++i];
        }
    }

    if (!message){
        fprintf(stderr, "usage: ./git_clone commit -m <message>\n");
        return false;
    }

    Repository *repo = repo_find(".", true);
    if (!repo) return false;

    GitIndex *idx = index_read(repo);
    char *tree_sha = tree_from_index(repo, idx);
    index_destroy(idx);

    if (!tree_sha){
        fprintf(stderr, "commit: failed to build tree from index\n");
        repo_destroy(repo);
        return false;
    }

    char *parent_sha = ref_resolve(repo, "HEAD");
    char *author = gitconfig_user_get();
    if (!author){ author = safe_strdup("uknown <unknown@example.com"); }

    char *commit_sha = commit_create(repo, tree_sha, parent_sha, author, time(NULL), message);

    free(tree_sha);
    free(parent_sha);
    free(author);
    
    char *branch = branch_get_active(repo);
    bool ok;
    if (branch){
        char *ref_name = path_join("refs/heads", branch, NULL);
        ok = ref_create(repo, ref_name, commit_sha);
        free(ref_name);
        free(branch);
    } else {
        char *head_path = repo_file(repo, false, "HEAD", NULL);
        FILE *f = safe_fopen(head_path, "w");
        fprintf(f, "%s\n", commit_sha);
        fclose(f);
        free(head_path);
        ok = true;
    }

    printf("%s\n", commit_sha);
    free(commit_sha);
    repo_destroy(repo);
    return ok;
}