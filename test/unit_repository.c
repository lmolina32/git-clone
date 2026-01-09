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

int test_02_repo_path(){
    printf("Running file_exists extended tests...\n");

    const char *test_path = "test_repo_zone";

    Repository *repo = repo_create(test_path, true);
    assert(repo != NULL);

    // Test 1: Join one string 
    char *s1 = repo_path(repo, "tags", NULL);
    assert(s1 != NULL);
    assert(streq(s1, "test_repo_zone/.git/tags"));
    printf("Test 1 passed: Simple join\n");
    free(s1);

    // Test 2: Repo only path 
    char *s2 = repo_path(repo, NULL);
    assert(s2 != NULL);
    assert(streq(s2, "test_repo_zone/.git"));
    printf("Test 2 passed: Repo path only\n");
    free(s2);

    // Test 3: Passing multiple strings  
    char *s3 = repo_path(repo, "refs", "heads", "main", NULL);
    assert(s3 != NULL);
    assert(streq(s3, "test_repo_zone/.git/refs/heads/main"));
    printf("Test 3 passed: Multiple string path\n");
    free(s3);

    // Test 4: NULL safety  
    char *s4 = repo_path(NULL);
    assert(s4 == NULL);
    printf("Test 4 passed: NULL safety \n");

    repo_destroy(repo);
    
    printf("\nAll path_join tests passed successfully!\n");

    return EXIT_SUCCESS;
}

int test_03_repo_dir() {
    printf("Running repo_dir tests...\n");

    const char *worktree = "test_wd";
    const char *gitdir = "test_wd/.git";
    const char *objdir = "test_wd/.git/objects";
    
    mkdir(worktree, 0755);
    mkdir(gitdir, 0755);
    mkdir(objdir, 0755);
    
    Repository repo;
    strcpy(repo.worktree, worktree);
    strcpy(repo.gitdir, gitdir);

    // --- Test 1: Accessing an existing directory ---
    char *res1 = repo_dir(&repo, false, "objects", NULL);
    assert(res1 != NULL);
    assert(streq(res1, "test_wd/.git/objects"));
    printf("Test 1 Passed: Found existing directory\n");
    free(res1);

    // --- Test 2: Creating a missing nested directory ---
    char *res2 = repo_dir(&repo, true, "refs", "tags", NULL);
    assert(res2 != NULL);
    struct stat sb;
    assert(stat(res2, &sb) == 0 && S_ISDIR(sb.st_mode));
    assert(streq(res2, "test_wd/.git/refs/tags"));
    printf("Test 2 Passed: Created missing nested directory\n");
    free(res2);

    // --- Test 3: Failure when mkdir=false on missing path ---
    char *res3 = repo_dir(&repo, false, "branches", NULL);
    assert(res3 == NULL);
    printf("Test 3 Passed: Correctly returned NULL for missing dir when mkdir=false\n");

    // --- Test 4: Failure when path is blocked by a file ---
    char *file_path = path_join(gitdir, "blocked_dir");
    FILE *f = fopen(file_path, "w");
    fclose(f);
    char *res4 = repo_dir(&repo, true, "blocked_dir", NULL);
    assert(res4 == NULL); 
    printf("Test 4 Passed: Handled path blocked by file correctly\n");

    remove(file_path);
    free(file_path);
    rmdir("test_wd/.git/refs/tags");
    rmdir("test_wd/.git/refs");
    rmdir("test_wd/.git/objects");
    rmdir("test_wd/.git");
    rmdir("test_wd");

    printf("\nAll repo_dir tests passed successfully!\n");
    return EXIT_SUCCESS;
}

int test_04_repo_file(){
    printf("Running repo_file tests...\n");

    const char *worktree = "test_wd_file";
    const char *gitdir = "test_wd_file/.git";
    struct stat sb;

    // 1. Setup: Create mock repository
    mkdir(worktree, 0755);
    mkdir(gitdir, 0755);
    Repository repo;
    strncpy(repo.worktree, worktree, sizeof(repo.worktree)-1);
    strncpy(repo.gitdir, gitdir, sizeof(repo.gitdir)-1);

    // --- Test 1: Simple file in a new directory ---
    char *res1 = repo_file(&repo, true, "logs", "HEAD", NULL);
    assert(res1 != NULL);
    assert(streq(res1, "test_wd_file/.git/logs/HEAD"));
    assert(stat("test_wd_file/.git/logs", &sb) == 0 && S_ISDIR(sb.st_mode));
    assert(stat("test_wd_file/.git/logs/HEAD", &sb) != 0); 
    printf("Test 1 Passed: Created parent directory for file\n");
    free(res1);

    // --- Test 2: Deeply nested file ---
    char *res2 = repo_file(&repo, true, "refs", "remotes", "origin", "main", NULL);
    assert(res2 != NULL);
    assert(stat("test_wd_file/.git/refs/remotes/origin", &sb) == 0);
    printf("Test 2 Passed: Created deep nested parent directories\n");
    free(res2);

    // --- Test 3: mkdir=false on missing directory ---
    char *res3 = repo_file(&repo, false, "info", "exclude", NULL);
    assert(res3 == NULL);
    printf("Test 3 Passed: Correctly failed when mkdir=false and path missing\n");

    remove("test_wd_file/.git/logs/HEAD");
    rmdir("test_wd_file/.git/logs");
    rmdir("test_wd_file/.git/refs/remotes/origin");
    rmdir("test_wd_file/.git/refs/remotes");
    rmdir("test_wd_file/.git/refs");
    rmdir("test_wd_file/.git");
    rmdir("test_wd_file");

    printf("\nAll repo_file tests passed successfully!\n");
    return EXIT_SUCCESS;
}

int test_05_repo_init() {
    printf("Running repo_init tests...\n");

    const char *test_path = "test_new_git_repo";
    struct stat sb;

    // --- Test 1: Successful Initialization ---
    Repository *repo = repo_init(test_path);
    
    assert(repo != NULL);
    printf("Test 1a Passed: repo_init returned a valid Repository pointer\n");

    // Check Directories
    const char *dirs[] = {
        "test_new_git_repo/.git/branches",
        "test_new_git_repo/.git/objects",
        "test_new_git_repo/.git/refs/tags",
        "test_new_git_repo/.git/refs/heads"
    };

    for (int i = 0; i < 4; i++) {
        assert(stat(dirs[i], &sb) == 0 && S_ISDIR(sb.st_mode));
    }
    printf("Test 1b Passed: All internal .git directories created\n");

    // Check Files and Content
    char *head_path = path_join(test_path, ".git/HEAD");
    FILE *f_head = fopen(head_path, "r");
    assert(f_head != NULL);
    char line[256];
    if(fgets(line, sizeof(line), f_head)){}
    assert(strstr(line, "ref: refs/heads/master") != NULL);
    fclose(f_head);
    free(head_path);
    printf("Test 1c Passed: HEAD file initialized correctly\n");

    char *config_path = path_join(test_path, ".git/config");
    assert(file_exists(config_path));
    free(config_path);
    printf("Test 1d Passed: Config file created\n");

    // --- Test 2: Failure on Non-Empty Directory ---
    Repository *repo_fail = repo_init(test_path);
    assert(repo_fail == NULL);
    printf("Test 2 Passed: Correctly refused to overwrite non-empty .git\n");

    // --- Teardown ---
    remove("test_new_git_repo/.git/HEAD");
    remove("test_new_git_repo/.git/config");
    remove("test_new_git_repo/.git/description");
    rmdir("test_new_git_repo/.git/branches");
    rmdir("test_new_git_repo/.git/objects");
    rmdir("test_new_git_repo/.git/refs/tags");
    rmdir("test_new_git_repo/.git/refs/heads");
    rmdir("test_new_git_repo/.git/refs");
    rmdir("test_new_git_repo/.git");
    rmdir("test_new_git_repo");
    
    repo_destroy(repo);

    printf("\nAll repo_init tests passed successfully!\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is right of the following:\n");
        fprintf(stderr, "    0. Test repo_config_create\n");
        fprintf(stderr, "    1. Test repo_create\n");
        fprintf(stderr, "    2. Test repo_path\n");
        fprintf(stderr, "    3. Test repo_dir\n");
        fprintf(stderr, "    4. Test repo_file\n");
        fprintf(stderr, "    5. Test repo_init\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_repo_config_create(); break;
        case 1:  status = test_01_repo_create(); break;
        case 2:  status = test_02_repo_path(); break;
        case 3:  status = test_03_repo_dir(); break;
        case 4:  status = test_04_repo_file(); break;
        case 5:  status = test_05_repo_init(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}