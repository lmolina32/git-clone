#include "status.h"
#include "repository.h"
#include "objects.h"
#include "tree.h"
#include "utils.h"
#include "index.h"
#include "gitignore.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Helpers */

static Repository *create_temp_repo(char *out_path_buf, size_t buf_size) {
    snprintf(out_path_buf, buf_size, "/tmp/test_status_XXXXXX");
    char *dir = mkdtemp(out_path_buf);
    assert(dir != NULL);
    Repository *repo = repo_init(dir);
    assert(repo != NULL);
    return repo;
}

static void destroy_temp_repo(Repository *repo) {
    if (!repo) return;
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", repo->worktree);
    (void)system(cmd);
    repo_destroy(repo);
}

static void write_file(const char *path, const char *content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);
    size_t len = strlen(content);
    assert(write(fd, content, len) == (ssize_t)len);
    close(fd);
}

static char *create_blob(Repository *repo, const char *data) {
    Object *blob = object_new(GIT_BLOB, (char *)data, strlen(data));
    assert(blob != NULL);
    char *sha = object_write(blob, repo);
    object_destroy(blob);
    return sha;
}

/* Tests */

int test_00_pathmap_basic() {
    printf("Running pathmap set/find/remove test...\n");
    PathMap m = {0};

    pathmap_set(&m, "file.txt", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    pathmap_set(&m, "dir/file2.txt", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    assert(m.count == 2);

    PathEntry *e = pathmap_find(&m, "file.txt");
    assert(e != NULL);
    assert(strcmp(e->sha, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") == 0);

    pathmap_set(&m, "file.txt", "cccccccccccccccccccccccccccccccccccccccc");
    e = pathmap_find(&m, "file.txt");
    assert(strcmp(e->sha, "cccccccccccccccccccccccccccccccccccccccc") == 0);

    pathmap_remove(&m, "file.txt");
    assert(m.count == 1);
    assert(pathmap_find(&m, "file.txt") == NULL);

    pathmap_destroy(&m);
    printf("Test 0 Passed: pathmap set/find/remove work\n");
    return EXIT_SUCCESS;
}

int test_01_tree_to_dict() {
    printf("Running tree_to_dict test...\n");
    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    char *sha1 = create_blob(repo, "content1");
    char *sha2 = create_blob(repo, "content2");

    Tree *tree = tree_new();
    tree_add_entry(tree, "100644", "hello.txt", sha1);
    tree_add_entry(tree, "100644", "subdir/world.txt", sha2);
    Object *tree_obj = tree_to_object(tree);
    char *tree_sha = object_write(tree_obj, repo);
    tree_destroy(tree);
    object_destroy(tree_obj);

    PathMap map = {0};
    bool ok = tree_to_dict(repo, tree_sha, "", &map);
    assert(ok);
    assert(map.count == 2);

    PathEntry *e1 = pathmap_find(&map, "hello.txt");
    PathEntry *e2 = pathmap_find(&map, "subdir/world.txt");
    assert(e1 != NULL && strcmp(e1->sha, sha1) == 0);
    assert(e2 != NULL && strcmp(e2->sha, sha2) == 0);

    pathmap_destroy(&map);
    free(sha1); free(sha2); free(tree_sha);
    destroy_temp_repo(repo);
    printf("Test 1 Passed: tree_to_dict flattens tree\n");
    return EXIT_SUCCESS;
}

int test_02_branch_get_active() {
    printf("Running branch_get_active test...\n");
    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    char *head_path = repo_file(repo, false, "HEAD", NULL);
    assert(head_path != NULL);
    write_file(head_path, "ref: refs/heads/main\n");
    free(head_path);

    char *branch = branch_get_active(repo);
    assert(branch != NULL);
    assert(strcmp(branch, "main") == 0);
    free(branch);

    head_path = repo_file(repo, false, "HEAD", NULL);
    write_file(head_path, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
    free(head_path);

    branch = branch_get_active(repo);
    assert(branch == NULL);

    destroy_temp_repo(repo);
    printf("Test 2 Passed: branch_get_active resolves branch and detached HEAD\n");
    return EXIT_SUCCESS;
}

int test_03_walk_tree() {
    printf("Running walk_tree test...\n");
    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    char *file1 = path_join(repo->worktree, "file1.txt", NULL);
    char *dir = path_join(repo->worktree, "subdir", NULL);
    char *file2 = path_join(repo->worktree, "subdir/file2.txt", NULL);
    mkdir(dir, 0755);
    write_file(file1, "hello");
    write_file(file2, "world");
    free(file1); free(dir); free(file2);

    // Create a file inside .git that should be skipped
    char *gitfile = path_join(repo->gitdir, "config", NULL);
    write_file(gitfile, "skip me");
    free(gitfile);

    StrList files;
    string_list_init(&files);
    walk_tree(repo, repo->worktree, &files);

    assert(files.count == 2);
    bool has1 = string_list_remove(&files, "file1.txt");
    bool has2 = string_list_remove(&files, "subdir/file2.txt");
    assert(has1 && has2);
    assert(files.count == 0);

    string_list_destroy(&files);
    destroy_temp_repo(repo);
    printf("Test 3 Passed: walk_tree collects worktree files and skips .git\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is one of:\n");
        fprintf(stderr, "  0. pathmap set/find/remove\n");
        fprintf(stderr, "  1. tree_to_dict\n");
        fprintf(stderr, "  2. branch_get_active\n");
        fprintf(stderr, "  3. walk_tree\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    switch (number) {
        case 0: return test_00_pathmap_basic();
        case 1: return test_01_tree_to_dict();
        case 2: return test_02_branch_get_active();
        case 3: return test_03_walk_tree();
        default:
            fprintf(stderr, "Unknown NUMBER: %d\n", number);
            return EXIT_FAILURE;
    }
}