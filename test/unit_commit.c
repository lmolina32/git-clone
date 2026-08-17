#include "commit.h"
#include "repository.h"
#include "objects.h"
#include "kvlm.h"
#include "tree.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

int test_00_commit_create_basic(void) {
    printf("Running commit_create basic test...\n");
    char template[] = "/tmp/git_clone_test_XXXXXX";
    char *repo_path = mkdtemp(template);
    assert(repo_path != NULL);
    Repository *repo = repo_init(repo_path);
    assert(repo != NULL);

    /* Create a dummy tree object to reference */
    Tree *t = tree_new();
    tree_add_entry(t, "100644", "dummy", "0000000000000000000000000000000000000000");
    Object *tree_obj = tree_to_object(t);
    char *tree_sha = object_write(tree_obj, repo);
    tree_destroy(t);
    object_destroy(tree_obj);

    const char *author = "John Doe <john@example.com>";
    time_t now = time(NULL);
    const char *message = "Initial commit";

    char *commit_sha = commit_create(repo, tree_sha, NULL, author, now, message);
    assert(commit_sha != NULL);
    assert(strlen(commit_sha) == 40);

    /* Read back the commit and verify its contents */
    Object *obj = object_read(repo, commit_sha);
    assert(obj != NULL);
    assert(obj->type == GIT_COMMIT);

    KVLM *kvlm = kvlm_parse(obj->data, obj->size);
    assert(kvlm != NULL);

    const char *tree_val = kvlm_get(kvlm, "tree");
    assert(streq(tree_val, tree_sha));

    const char *parent_val = kvlm_get(kvlm, "parent");
    assert(parent_val == NULL); /* no parent */

    const char *author_val = kvlm_get(kvlm, "author");
    assert(strstr(author_val, author) != NULL);

    const char *msg = kvlm_get(kvlm, NULL);
    assert(streq(msg, "Initial commit\n"));

    kvlm_destroy(kvlm);
    object_destroy(obj);
    free(commit_sha);
    free(tree_sha);
    repo_destroy(repo);
    remove_directory(repo_path);
    /* repo_path is stack memory, do NOT free() it */
    printf("Test 0 Passed: commit_create creates a valid commit object\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "  0. commit_create basic\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;
    switch (number) {
        case 0: status = test_00_commit_create_basic(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }
    return status;
}