#include "repository.h"
#include "utils.h"
 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int test_00_repo_config_create() {
    printf("Running repo_config_create tests...\n");

    const char *config_path = "test_config.ini";

    // 1. Setup: Create a valid git-style config file
    FILE *f = fopen(config_path, "w");
    fprintf(f, "[core]\n");
    fprintf(f, "repositoryformatversion = 0\n");
    fprintf(f, "filemode = true\n");
    fprintf(f, "bare = false\n");
    fclose(f);

    Configuration *config = repo_config_create(config_path);
    
    // Test 1: Successful allocation and parsing
    assert(config != NULL);
    printf("Test 1 Passed: Configuration object created\n");

    // Test 2: Verify repositoryformatversion (Integer conversion)
    assert(config->repo_format_version == 0);
    printf("Test 2 Passed: repo_format_version is 0\n");

    // Test 3: Verify filemode (Boolean "true" -> 1)
    assert(config->filemode == 1);
    printf("Test 3 Passed: filemode 'true' converted to 1\n");

    // Test 4: Verify bare (Boolean "false" -> 0)
    assert(config->bare == 0);
    printf("Test 4 Passed: bare 'false' converted to 0\n");

    // Test 5: Handling non-existent file
    Configuration *bad_config = repo_config_create("missing_file.ini");
    assert(bad_config == NULL);
    printf("Test 5 Passed: Handled missing file correctly\n");

    free(config);
    remove(config_path);

    printf("\nAll repo_config_create tests passed successfully!\n");
    return EXIT_SUCCESS;
}

int test_01_repo_create() {
    printf("Running repo_create tests...\n");

    const char *test_path = "test_repo_zone";
    const char *git_path = "test_repo_zone/.git";
    const char *config_path = "test_repo_zone/.git/config";

    // --- Test 1: Force Create (No files exist yet) ---
    Repository *repo_forced = repo_create(test_path, true);
    assert(repo_forced != NULL);
    assert(streq(repo_forced->worktree, test_path));
    assert(streq(repo_forced->gitdir, git_path));
    printf("Test 1 Passed: Force create successful\n");
    repo_destroy(repo_forced);

    // --- Test 2: Validation Failure (No .git folder) ---
    Repository *repo_fail = repo_create(test_path, false);
    assert(repo_fail == NULL);
    printf("Test 2 Passed: Correctly failed on missing .git\n");

    // --- Test 3: Valid Repository Initialization ---
    mkdir(test_path, 0755);
    mkdir(git_path, 0755);
    FILE *f = fopen(config_path, "w");
    fprintf(f, "[core]\nrepositoryformatversion = 0\nfilemode = true\nbare = false\n");
    fclose(f);

    Repository *repo_valid = repo_create(test_path, false);
    assert(repo_valid != NULL);
    assert(streq(repo_valid->worktree, test_path));
    assert(streq(repo_valid->gitdir, git_path));
    assert(repo_valid->config != NULL);
    assert(repo_valid->config->repo_format_version == 0);
    assert(repo_valid->config->filemode == 1);
    assert(repo_valid->config->bare == 0);
    printf("Test 3 Passed: Valid repository recognized\n");
    repo_destroy(repo_valid);

    // --- Test 4: Unsupported Version ---
    f = fopen(config_path, "w");
    fprintf(f, "[core]\nrepositoryformatversion = 1\n"); // Version 1 is unsupported
    fclose(f);

    Repository *repo_v1 = repo_create(test_path, false);
    assert(repo_v1 == NULL);
    printf("Test 4 Passed: Correctly rejected unsupported version 1\n");

    // --- Cleanup ---
    remove(config_path);
    rmdir(git_path);
    rmdir(test_path);

    printf("\nAll repo_create tests passed successfully!\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is right of the following:\n");
        fprintf(stderr, "    0. Test repo_config_create\n");
        fprintf(stderr, "    0. Test repo_create\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_repo_config_create(); break;
        case 1:  status = test_01_repo_create(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}