/* unit_stage.c: unit tests for stage.c */

#include "stage.h"
#include "index.h"
#include "repository.h"
#include "objects.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

/* Helpers */

static char *create_temp_dir(void) {
    char template[] = "/tmp/git_clone_test_XXXXXX";
    char *dir = mkdtemp(template);
    assert(dir != NULL);
    return strdup(dir);  
}

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    fprintf(f, "%s", content);
    fclose(f);
}

/* Tests */

int test_00_stage_add_single_file(void) {
    printf("Running stage_add single file test...\n");
    char *repo_path = create_temp_dir();
    Repository *repo = repo_init(repo_path);
    assert(repo != NULL);

    char *file_path = path_join(repo_path, "hello.txt", NULL);
    write_file(file_path, "Hello, world!");

    const char *paths[] = { "hello.txt" };
    bool ok = stage_add(repo, paths, 1);
    assert(ok);

    GitIndex *idx = index_read(repo);
    assert(idx->count == 1);
    assert(streq(idx->entries[0].name, "hello.txt"));
    assert(strlen(idx->entries[0].sha) == 40);

    index_destroy(idx);
    repo_destroy(repo);
    remove_directory(repo_path);
    free(repo_path);
    free(file_path);
    printf("Test 0 Passed: stage_add adds a file to the index\n");
    return EXIT_SUCCESS;
}

int test_01_stage_remove_single_file(void) {
    printf("Running stage_remove single file test...\n");
    char *repo_path = create_temp_dir();
    Repository *repo = repo_init(repo_path);
    assert(repo != NULL);

    char *file_path = path_join(repo_path, "remove_me.txt", NULL);
    write_file(file_path, "To be removed");

    const char *paths[] = { "remove_me.txt" };
    bool ok = stage_add(repo, paths, 1);
    assert(ok);

    /* Now remove it (delete_files = false) */
    ok = stage_remove(repo, paths, 1, false, false);
    assert(ok);

    GitIndex *idx = index_read(repo);
    assert(idx->count == 0);
    index_destroy(idx);

    /* File should still exist because delete_files=false */
    assert(file_exists(file_path));

    /* Now remove with delete_files=true */
    ok = stage_remove(repo, paths, 1, true, false);
    assert(ok);
    assert(!file_exists(file_path));

    repo_destroy(repo);
    remove_directory(repo_path);
    free(repo_path);
    free(file_path);
    printf("Test 1 Passed: stage_remove removes index entry and optionally file\n");
    return EXIT_SUCCESS;
}

int test_02_stage_remove_skip_missing(void) {
    printf("Running stage_remove skip_missing test...\n");
    char *repo_path = create_temp_dir();
    Repository *repo = repo_init(repo_path);
    assert(repo != NULL);

    const char *paths[] = { "non_existent.txt" };
    /* Should fail with skip_missing=false */
    bool ok = stage_remove(repo, paths, 1, false, false);
    assert(!ok);

    /* Should succeed with skip_missing=true */
    ok = stage_remove(repo, paths, 1, false, true);
    assert(ok);

    repo_destroy(repo);
    remove_directory(repo_path);
    free(repo_path);
    printf("Test 2 Passed: stage_remove respects skip_missing\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "  0. stage_add single file\n");
        fprintf(stderr, "  1. stage_remove single file (with/without delete)\n");
        fprintf(stderr, "  2. stage_remove skip_missing\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;
    switch (number) {
        case 0: status = test_00_stage_add_single_file(); break;
        case 1: status = test_01_stage_remove_single_file(); break;
        case 2: status = test_02_stage_remove_skip_missing(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }
    return status;
}