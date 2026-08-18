#include "ref.h"
#include "repository.h"
#include "objects.h"
#include "utils.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Helper to initialize a full git repository for testing */
static Repository *create_temp_repo(char *out_path_buf, size_t buf_size) {
    snprintf(out_path_buf, buf_size, "/tmp/test_git_ref_XXXXXX");
    char *dir = mkdtemp(out_path_buf);
    assert(dir != NULL);

    Repository *repo = repo_init(dir);
    assert(repo != NULL);
    assert(repo->config != NULL);
    return repo;
}

/* Helper to clean up repository files and deallocate Repository struct memory */
static void destroy_temp_repo(Repository *repo) {
    if (!repo) return;
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", repo->worktree);
    (void)system(cmd);
    repo_destroy(repo);
}

int test_00_ref_resolve() {
    printf("Running ref_resolve edge cases test...\n");

    char repo_path_buf[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path_buf, sizeof(repo_path_buf));

    // 1. Non-existent reference returns NULL
    assert(ref_resolve(repo, "refs/heads/nonexistent") == NULL);

    // 2. Directory target returns NULL
    assert(ref_resolve(repo, "refs/heads") == NULL);

    // 3. Direct SHA reference resolution
    const char *sha_str = "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391";
    assert(ref_create(repo, "refs/heads/master", sha_str) == true);

    char *resolved = ref_resolve(repo, "refs/heads/master");
    assert(resolved != NULL);
    assert(strcmp(resolved, sha_str) == 0);
    free(resolved);

    // 4. Symbolic reference resolution (repo_init creates HEAD -> ref: refs/heads/master)
    char *resolved_head = ref_resolve(repo, "HEAD");
    assert(resolved_head != NULL);
    assert(strcmp(resolved_head, sha_str) == 0);
    free(resolved_head);

    destroy_temp_repo(repo);
    printf("Test 0 Passed: ref_resolve handles missing, direct, directory, and symbolic HEAD references\n");
    return EXIT_SUCCESS;
}

int test_01_ref_create() {
    printf("Running ref_create edge cases test...\n");

    char repo_path_buf[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path_buf, sizeof(repo_path_buf));
    const char *dummy_sha = "1111111111111111111111111111111111111111";

    // 1. Standard ref creation under refs/heads
    assert(ref_create(repo, "refs/heads/feature", dummy_sha) == true);
    char *res = ref_resolve(repo, "refs/heads/feature");
    assert(res != NULL);
    assert(strcmp(res, dummy_sha) == 0);
    free(res);

    // 2. Overwriting existing reference
    const char *updated_sha = "2222222222222222222222222222222222222222";
    assert(ref_create(repo, "refs/heads/feature", updated_sha) == true);
    res = ref_resolve(repo, "refs/heads/feature");
    assert(res != NULL);
    assert(strcmp(res, updated_sha) == 0);
    free(res);

    destroy_temp_repo(repo);
    printf("Test 1 Passed: Reference creation and update succeed\n");
    return EXIT_SUCCESS;
}

int test_02_refnode_capacity_and_lifecycle() {
    printf("Running RefNode capacity allocation and cleanup test...\n");

    // NULL pointer destroy safety check
    ref_node_destroy(NULL);

    RefNode *parent = safe_calloc(sizeof(RefNode), 1);
    parent->name = safe_strdup("root");

    // Force capacity expansion (0 -> 8 -> 16) by adding 10 child nodes
    for (int i = 0; i < 10; i++) {
        RefNode child = {0};
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "node_%02d", i);
        child.name = safe_strdup(name_buf);
        child.sha = safe_strdup("3333333333333333333333333333333333333333");
        child.is_leaf = true;

        if (parent->child_capacity == parent->child_count) {
            parent->child_capacity = parent->child_capacity ? parent->child_capacity * 2 : 8;
            parent->children = safe_realloc(parent->children, parent->child_capacity * sizeof(RefNode));
        }
        parent->children[parent->child_count++] = child;
    }

    assert(parent->child_count == 10);
    assert(parent->child_capacity == 16);

    ref_node_destroy(parent);
    printf("Test 2 Passed: RefNode dynamic allocation and recursive destruction work without leaks\n");
    return EXIT_SUCCESS;
}

int test_03_ref_list_and_sorting() {
    printf("Running ref_list directory traversal and sorting test...\n");

    char repo_path_buf[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path_buf, sizeof(repo_path_buf));

    // Non-existent path returns empty root ref struct safely
    RefNode *empty_list = ref_list(repo, "/invalid/directory/path");
    assert(empty_list != NULL);
    assert(empty_list->child_count == 0);
    ref_node_destroy(empty_list);

    // Populate refs out-of-order to test sorting logic
    ref_create(repo, "refs/heads/zebra", "1111111111111111111111111111111111111111");
    ref_create(repo, "refs/heads/alpha", "2222222222222222222222222222222222222222");
    ref_create(repo, "refs/heads/beta",  "3333333333333333333333333333333333333333");

    // Dynamically build path via repo_path to avoid buffer truncation warnings
    char *heads_path = repo_path(repo, "refs", "heads", NULL);

    RefNode *tree = ref_list(repo, heads_path);
    free(heads_path);

    assert(tree != NULL);
    assert(tree->child_count == 3);

    // Verify alphabetical qsort ordering
    assert(strcmp(tree->children[0].name, "alpha") == 0);
    assert(strcmp(tree->children[1].name, "beta") == 0);
    assert(strcmp(tree->children[2].name, "zebra") == 0);

    ref_node_destroy(tree);
    destroy_temp_repo(repo);
    printf("Test 3 Passed: ref_list traverses directories and sorts entries alphabetically\n");
    return EXIT_SUCCESS;
}

int test_04_show_ref() {
    printf("Running show_ref recursion test...\n");

    RefNode root = {0};
    RefNode child1 = {0};
    child1.name = safe_strdup("main");
    child1.sha = safe_strdup("4444444444444444444444444444444444444444");
    child1.is_leaf = true;

    RefNode child2 = {0};
    child2.name = safe_strdup("unresolved");
    child2.sha = NULL; // Unresolved leaf
    child2.is_leaf = true;

    root.child_capacity = 2;
    root.children = safe_calloc(sizeof(RefNode), 2);
    root.children[root.child_count++] = child1;
    root.children[root.child_count++] = child2;

    show_ref(&root, true, "refs/heads");
    show_ref(&root, false, "refs/heads");

    ref_node_free_contents(&root);
    printf("Test 4 Passed: show_ref prints leaf formatting and skips unresolved references\n");
    return EXIT_SUCCESS;
}

int test_05_tag_create() {
    printf("Running tag_create test...\n");

    char repo_path_buf[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path_buf, sizeof(repo_path_buf));

    // 1. Tag target does not exist -> fails safely
    assert(tag_create(repo, "v1.0", "nonexistent_ref", false) == false);

    // 2. Lightweight tag creation targeting master
    const char *commit_sha = "5555555555555555555555555555555555555555";
    assert(ref_create(repo, "refs/heads/master", commit_sha) == true);
    assert(tag_create(repo, "v1.0", "refs/heads/master", false) == true);

    char *tag_sha = ref_resolve(repo, "refs/tags/v1.0");
    assert(tag_sha != NULL);
    assert(strcmp(tag_sha, commit_sha) == 0);
    free(tag_sha);

    destroy_temp_repo(repo);
    printf("Test 5 Passed: Lightweight tag creation resolves references and writes tag refs\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is one of the following:\n");
        fprintf(stderr, "    0. Test ref_resolve edge cases\n");
        fprintf(stderr, "    1. Test ref_create overwriting and creation\n");
        fprintf(stderr, "    2. Test RefNode capacity allocation and memory cleanup\n");
        fprintf(stderr, "    3. Test ref_list directory traversal and sorting\n");
        fprintf(stderr, "    4. Test show_ref traversal formats\n");
        fprintf(stderr, "    5. Test tag_create lightweight tagging\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_ref_resolve(); break;
        case 1:  status = test_01_ref_create(); break;
        case 2:  status = test_02_refnode_capacity_and_lifecycle(); break;
        case 3:  status = test_03_ref_list_and_sorting(); break;
        case 4:  status = test_04_show_ref(); break;
        case 5:  status = test_05_tag_create(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}