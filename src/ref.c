/* ref.c: git references (Branches, tags, and HEAD) */

#include "ref.h"
#include "objects.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>


/**
 * ref_resolve - resolves a Git reference string to a 40-character SHA-1 hash
 * 
 * Looks up the specified reference file in the repository. If the file contains a
 * symbolic reference starting with "ref: ", the function recursively resolves the
 * target reference. Otherwise, it strips trailing whitespace and returns the SHA-1 hash.
 * 
 * @param repo Pointer to the target Repository struct.
 * @param ref  The reference path or name to resolve (e.g., "HEAD", "refs/heads/main").
 *
 * @return Pointer to a dynamically allocated 40-character SHA-1 hexadecimal string,
 *         or NULL if the reference does not exist, is a directory, or fails to open.
 **/
char *ref_resolve(Repository *repo, const char *ref){
    char *path = repo_file(repo, false, ref, NULL);
    if (!path) return NULL;

    if (!file_exists(path) || is_directory(path)){
        free(path);
        return NULL;
    }

    FILE *f = safe_fopen(path, "r");
    free(path);
    if (!f) return NULL;

    char buf[BUFSIZ];
    bool got = fgets(buf, sizeof(buf), f) != NULL;
    fclose(f);
    if (!got) return NULL;
    chomp(buf);


    if (strneq(buf, "ref: ", 5)){
        return ref_resolve(repo, buf+ 5);
    }
    return safe_strdup(buf);
}

/**
 * refnode_add_child - appends a child RefNode to a parent RefNode's children array
 * 
 * @param parent Pointer to the parent RefNode structure to modify.
 * @param child  The RefNode structure to append to the children list.
 *
 * @return Void.
 **/
static void refnode_add_child(RefNode *parent, RefNode child){
    if (parent->child_capacity == parent->child_count){
        parent->child_capacity = parent->child_capacity ? parent->child_capacity * 2 : 8;
        parent->children = safe_realloc(parent->children, parent->child_capacity * sizeof(RefNode));
    }
    parent->children[parent->child_count++] = child;
}

/**
 * name_cmp - qsort comparison callback for RefNode structures by name
 * 
 * @param a Pointer to the first RefNode.
 * @param b Pointer to the second RefNode.
 *
 * @return Negative integer if a < b, 0 if equal, positive integer if a > b.
 **/
static int name_cmp(const void *a, const void *b){
    return strcmp(((const RefNode *)a)->name, ((const RefNode *)b)->name);
}

/**
 * ref_list - recursively lists and builds a tree representation of repository references
 * 
 * Traverses the specified directory (or default 'refs' directory within the repo).
 * Constructs a tree of RefNode structures representing subdirectories and leaf references,
 * resolving SHA-1 hashes for leaves, and sorting child nodes alphabetically.
 * 
 * @param repo     Pointer to the target Repository struct.
 * @param dir_path Directory path to traverse, or NULL to default to the repository's 'refs' directory.
 *
 * @return Pointer to the allocated root RefNode structure containing the hierarchical reference tree.
 **/
RefNode *ref_list(Repository *repo, const char *dir_path){
    char *path = dir_path ? safe_strdup(dir_path) : repo_dir(repo, false, "refs", NULL);

    RefNode *ref = safe_calloc(sizeof(RefNode), 1);
    if (!path) return ref;

    DIR *d = opendir(path);
    if (!d){
        free(path);
        return ref;
    }

    for (struct dirent *e = readdir(d); e; e = readdir(d)){
        if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue; 

        char *child_path = path_join(path, e->d_name, NULL);
        RefNode child = {0};
        child.name = safe_strdup(e->d_name);

        if (is_directory(child_path)){
            RefNode *sub = ref_list(repo, child_path);
            child.children = sub->children;
            child.child_count = sub->child_count;
            child.child_capacity = sub->child_capacity;
            free(sub);
        } else {
            char *rel = child_path + strlen(repo->gitdir) + 1;
            child.is_leaf = true; 
            child.sha = ref_resolve(repo, rel);
        }

        free(child_path);
        refnode_add_child(ref, child);
    }

    closedir(d);
    free(path);

    qsort(ref->children, ref->child_count, sizeof(RefNode), name_cmp);    
    return ref;
}

/**
 * ref_node_free_contents - recursively frees string fields and children of a RefNode
 * 
 * @param node Pointer to the RefNode whose contents should be freed.
 *
 * @return Void.
 **/
void ref_node_free_contents(RefNode *node){
    if (node->name){ free(node->name); }
    if (node->sha){ free(node->sha); }
    for (size_t i = 0; i < node->child_count; i++){
        ref_node_free_contents(&node->children[i]);
    }
    free(node->children);
}

/**
 * ref_node_destroy - safely deallocates a RefNode hierarchy
 * 
 * @param node Pointer to the root RefNode to destroy.
 *
 * @return Void.
 **/
void ref_node_destroy(RefNode *node){
    if (!node) return;
    ref_node_free_contents(node);
    free(node);
}

/**
 * show_ref - recursively prints formatted reference names and SHA-1 hashes to standard output
 * 
 * Iterates through a RefNode tree. For leaf nodes with resolved SHA-1 hashes, prints the node,
 * optionally prefixed by the SHA-1 hash if with_hash is set to true.
 * 
 * @param node      Pointer to the root or current subtree RefNode.
 * @param with_hash If true, prepends the SHA-1 hash and a space before each reference name.
 * @param prefix    Accumulated directory path prefix for nested reference formatting.
 *
 * @return Void.
 **/
void show_ref(RefNode *node, bool with_hash, const char *prefix){
    char pfx[MAX_PATH] = {0};
    if (prefix && *prefix){
        snprintf(pfx, sizeof(pfx), "%s/", prefix);
    }

    for (size_t i = 0; i < node->child_count; i++){
        RefNode *n = &node->children[i];
        if (n->is_leaf){
            if (!n->sha){ continue;}
            if (with_hash){ printf("%s %s%s\n", n->sha, pfx, n->name); }
            else { printf("%s%s\n", pfx, n->name); }
        } else {
            char child_pfx[MAX_PATH] = {0};
            snprintf(child_pfx, sizeof(child_pfx), "%s%s", prefix, n->name);
            show_ref(node, with_hash, child_pfx);
        }
    }
}

/**
 * ref_create - creates or updates a reference file with a SHA-1 hash
 * 
 * Opens the specified reference file within the repository for writing and stores the 
 * given SHA-1 hash followed by a newline.
 * 
 * @param repo     Pointer to the target Repository struct.
 * @param ref_name The path/name of the reference relative to the repository (e.g., "refs/heads/feature").
 * @param sha      The 40-character hexadecimal SHA-1 string to write into the reference file.
 *
 * @return True if the reference file was successfully written, false otherwise.
 **/
bool ref_create(Repository *repo, const char *ref_name, const char *sha){
    char *file_path = repo_file(repo, true, ref_name, NULL);
    if (!file_path) return false;

    FILE *f = safe_fopen(file_path, "w");
    fprintf(f, "%s\n", sha);
    fclose(f);
    free(file_path);
    return true;
}

/**
 * tag_create - creates a lightweight or annotated tag in the repository
 * 
 * Resolves the target reference to a SHA-1 hash. If create_tag_object is true, builds and
 * writes an annotated tag object into the object database first. Then writes a tag reference
 * under "refs/tags/<name>".
 * 
 * @param repo              Pointer to the target Repository struct.
 * @param name              Name of the tag to create.
 * @param ref               Target reference, object name, or SHA-1 to tag.
 * @param create_tag_object If true, generates an annotated tag object; if false, creates a lightweight tag.
 *
 * @return True on successful tag creation, false if target reference resolution or object creation fails.
 **/
bool tag_create(Repository *repo, const char *name, const char *ref, bool create_tag_object){
    char *sha = object_find(repo, ref, GIT_ANY_TYPE, true);
    if (!sha){
        fprintf(stderr, "tag: cannot resolve '%s'\n", ref);
        return false;
    }

    char *ref_name = path_join("tags", name, NULL);
    bool ok; 

    if (create_tag_object){
        KVLM *kvlm = kvlm_new();
        kvlm_set(kvlm, "object", sha);
        kvlm_set(kvlm, "type", "commit");
        kvlm_set(kvlm, "tag", name);
        // TODO: make this customizeable 
        kvlm_set(kvlm, "tagger", "git_clone <git@example.com>");
        kvlm_set(kvlm, NULL, "A tag to be generated by git clone\n");
        Object  *tag_obj = object_from_kvlm(GIT_TAG, kvlm);
        char *tag_sha = object_write(tag_obj, repo);
        ok = ref_create(repo, ref_name, tag_sha);
    } else {
        ok = ref_create(repo, ref_name, sha);
    }

    free(ref_name);
    free(sha);
    return ok;
}