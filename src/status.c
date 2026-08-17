/* status.c: */

#include "status.h"
#include "objects.h"
#include "tree.h"
#include "gitignore.h"
#include "utils.h"
#include "compat.h"

#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * pathmap_set - inserts or updates a path->SHA mapping in a PathMap
 *
 * If the path already exists, its SHA is overwritten. Otherwise a new
 * entry is added, growing the map as needed. The path string is
 * duplicated; the map owns the copy.
 *
 * @param m     Pointer to the PathMap.
 * @param path  Full relative path (used as key).
 * @param sha   40-character SHA-1 hex string (with NUL terminator).
 **/
void pathmap_set(PathMap *m, const char *path, const char *sha){
    for (size_t i = 0; i < m->count; i++){
        if (streq(m->entries[i].path, path)){
            memcpy(m->entries[i].sha, sha, 41);
            return;
        }
    }

    if (m->count == m->capacity){
        m->capacity = m->capacity ? m->capacity * 2 : 32;
        m->entries = safe_realloc(m->entries, m->capacity * sizeof(PathEntry));
    }

    m->entries[m->count].path = safe_strdup(path);
    memcpy(m->entries[m->count].sha, sha, 41);
    m->count++;
}

/**
 * pathmap_find - looks up a path in a PathMap
 *
 * @param m     Pointer to the PathMap.
 * @param path  Path to search for.
 *
 * @return Pointer to the matching PathEntry, or NULL if not found.
 **/
PathEntry *pathmap_find(PathMap *m, const char *path){
    for (size_t i = 0; i < m->count; i++){
        if (streq(m->entries[i].path, path)){
            return &m->entries[i];
        }
    }
    return NULL;
}

/**
 * pathmap_remove - removes a path from a PathMap
 *
 * Frees the stored path string and swaps the last entry into the
 * removed slot. Order is not preserved.
 *
 * @param m     Pointer to the PathMap.
 * @param path  Path to remove.
 **/
void pathmap_remove(PathMap *m, const char *path){
    for (size_t i = 0; i < m->count; i++){
        if (streq(m->entries[i].path, path)){
            free(m->entries[i].path);
            m->entries[i] = m->entries[m->count - 1];
            m->count--;
            return;
        }
    }
}

/**
 * pathmap_destroy - frees a PathMap and all its entries
 *
 * @param m  Pointer to the PathMap to destroy.
 **/
void pathmap_destroy(PathMap *m){
    for (size_t i = 0; i < m->count; i++){
        free(m->entries[i].path);
    }
    free(m->entries);
}

/**
 * tree_to_dict - flattens a tree object into a path->SHA PathMap
 *
 * Recursively traverses a tree (or any tree referenced by a ref),
 * converting nested tree entries into full filesystem paths and
 * mapping each blob SHA. Non-tree entries are stored directly.
 *
 * @param repo    Pointer to the Repository.
 * @param ref     Reference name or SHA that points to a tree.
 * @param prefix  Current directory prefix ("" for root).
 * @param out     Pointer to the PathMap to populate.
 *
 * @return true if the tree was successfully traversed, false on error.
 **/
bool tree_to_dict(Repository *repo, const char *ref, const char *prefix, PathMap *out){
    char *tree_sha = object_find(repo, ref, GIT_TREE, true);
    if (!tree_sha) return false;

    Object *obj = object_read(repo, tree_sha);
    free(tree_sha);
    if (!obj || obj->type != GIT_TREE){ object_destroy(obj); return false; }

    Tree *tree = object_to_tree(obj);
    object_destroy(obj);
    if (!tree) return false;

    bool ok = true;

    for (size_t i = 0; i < tree->count; i++){
        TreeLeaf *e = &tree->entries[i];
        char *full_path = (prefix && *prefix) ? path_join(prefix, e->path, NULL) : safe_strdup(e->path);

        if (streq(tree_entry_type(e->mode), "tree")){
            if (!tree_to_dict(repo, e->sha, full_path, out)) ok = false;
        } else {
            pathmap_set(out, full_path, e->sha);
        }
        free(full_path);
    }

    tree_destroy(tree);
    return ok;
}

/**
 * branch_get_active - returns the name of the active branch
 *
 * Reads .git/HEAD. If it contains an indirect reference
 * (ref: refs/heads/<name>), returns a newly allocated copy of the
 * branch name. Otherwise returns NULL, indicating a detached HEAD.
 *
 * @param repo  Pointer to the Repository.
 *
 * @return Heap-allocated branch name, or NULL if detached or error.
 **/
char *branch_get_active(Repository *repo){
    char *path = repo_file(repo, false, "HEAD", NULL);
    if (!path) return NULL;
    FILE *f = safe_fopen(path, "r");
    free(path);

    char buf[1<<10];
    bool got = fgets(buf, sizeof(buf), f) != NULL;
    fclose(f);
    if (!got) return NULL;
    chomp(buf);

    if (strneq(buf, "ref: refs/heads/", 16)){
        return safe_strdup(buf + 16);
    }
    return NULL;
}

/**
 * walk_tree - recursively collects files from the working tree
 *
 * Traverses the repository's worktree, skipping the .git directory,
 * and adds every file path (relative to the worktree root) to the
 * provided StrList.
 *
 * @param repo  Pointer to the Repository.
 * @param dir   Current directory to traverse.
 * @param out   Pointer to the StrList to fill.
 **/
void walk_tree(Repository *repo, const char *dir, StrList *out){
    if (streq(dir, repo->gitdir) || strneq(dir, repo->gitdir, strlen(repo->gitdir))){
        return;
    }

    DIR *d = opendir(dir);
    if (!d) return;
    
    for (struct dirent *e = readdir(d); e; e = readdir(d)){
        if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;

        char *full = path_join(dir, e->d_name, NULL);
        if (is_directory(full)){
            walk_tree(repo, full, out);
        } else {
            const char *rel = full + strlen(repo->worktree) + 1;
            string_list_add(out, rel);
        }
        free(full);
    }
    closedir(d);
}

/**
 * status_head_index - shows differences between HEAD and the index
 *
 * Compares the tree of HEAD with the index entries and prints a list
 * of staged changes: added, modified, and deleted files.
 *
 * @param repo  Pointer to the Repository.
 * @param idx   Pointer to the parsed GitIndex.
 **/
void status_head_index(Repository *repo, GitIndex *idx){
    printf("Changes to be committed:\n");

    PathMap head = {0};
    tree_to_dict(repo, "HEAD", "", &head);

    for (size_t i = 0; i < idx->count; i++){
        IndexEntry *e = &idx->entries[i];
        PathEntry *he = pathmap_find(&head, e->name);
        if (he){
            if (!streq(he->sha, e->sha)){
                printf("  modified: %s\n", e->name);
                pathmap_remove(&head, e->name);
            } else {
                printf("  added:    %s\n", e->name);
            }
        }
    }

    for (size_t i = 0; i < head.count; i++){
        printf("  deleted: %s\n", head.entries[i].path);
    }

    pathmap_destroy(&head);
}

/**
 * status_branch - prints the current branch or detached HEAD
 *
 * Displays "On branch <name>." when on a branch, or
 * "HEAD detached at <sha>" when in detached HEAD state.
 *
 * @param repo  Pointer to the Repository.
 **/
void status_branch(Repository *repo){
    char *branch = branch_get_active(repo);
    if (branch){
        printf("On branch %s.\n", branch);
        free(branch);
    } else {
        char *sha = object_find(repo, "HEAD", GIT_ANY_TYPE, true);
        printf("HEAD detached at %s\n", sha ? sha : "(unknown)");
        free(sha);
    }
}

/**
 * status_index_worktree - compares index with the working tree
 *
 * Determines which tracked files have been modified or deleted by
 * comparing file metadata and, when timestamps differ, actual file
 * contents. It also identifies untracked files that are not ignored.
 *
 * @param repo  Pointer to the Repository.
 * @param idx   Pointer to the parsed GitIndex.
 **/
void status_index_worktree(Repository *repo, GitIndex *idx){
    printf("Changes not staged for commit:\n");

    GitIgnore *ignore = gitignore_read(repo);

    StrList all_files;
    string_list_init(&all_files);
    walk_tree(repo, repo->worktree, &all_files);

    for (size_t i = 0; i < idx->count; i++){
        IndexEntry *e = &idx->entries[i];
        char *full_path = path_join(repo->worktree, e->name, NULL);

        struct stat sb;
        if (stat(full_path, &sb) != 0){
            printf("  deleted:  %s\n", e->name);
        } else {
            bool changed = ST_CTIME_SEC(sb)  != e->ctime_s  || 
                           ST_CTIME_NSEC(sb) != e->ctime_ns ||
                           ST_MTIME_SEC(sb)  != e->mtime_s  ||
                           ST_MTIME_NSEC(sb) != e->mtime_ns;

            if (changed){
                int fd = open(full_path, O_RDONLY);
                if (fd >= 0){
                    char *new_sha = object_hash(fd, GIT_BLOB, NULL);
                    close(fd);
                    if (new_sha){
                        if (!streq(new_sha, e->sha)){
                            printf("  modified: %s\n", e->name);
                        }
                        free(new_sha);
                    }
                }
            }
        }
        free(full_path);
        string_list_remove(&all_files, e->name);
    }

    printf("\nUntracked Files:\n");
    for (size_t i = 0; i < all_files.count; i++){
        if (!check_ignore(ignore, all_files.items[i])){
            printf("  %s\n", all_files.items[i]);
        }
    }

    string_list_destroy(&all_files);
    gitignore_destroy(ignore);
}
