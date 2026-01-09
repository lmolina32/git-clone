/* unit_utils.c: unit test utility functions */

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>


int test_00_path_join() {
    printf("Running str_join tests...\n");

    // Test 1: Joining two strings
    char *s1 = path_join("Hello", "World", NULL);
    assert(s1 != NULL);
    assert(streq(s1, "Hello/World"));
    printf("Test 1 Passed: Simple join\n");
    free(s1);

    // Test 2: Joining multiple strings (Path-like)
    char *s2 = path_join("/usr", "local", "bin", "git", NULL);
    assert(s2 != NULL);
    assert(streq(s2, "/usr/local/bin/git"));
    printf("Test 2 Passed: Multi-string join\n");
    free(s2);

    // Test 3: Joining with empty strings
    char *s3 = path_join("git", "", "init", NULL);
    assert(s3 != NULL);
    assert(streq(s3, "git/init"));
    printf("Test 3 Passed: Empty string handling\n");
    free(s3);

    // Test 4: Single argument (just the first string)
    char *s4 = path_join("Standalone", NULL);
    assert(s4 != NULL);
    assert(streq(s4, "Standalone"));
    printf("Test 4 Passed: Single string join\n");
    free(s4);

    // Test 5: Check for NULL return on NULL input
    char *s5 = path_join(NULL);
    assert(s5 == NULL);
    printf("Test 5 Passed: NULL safety\n");

    printf("\nAll path_join tests passed successfully!\n");

    return EXIT_SUCCESS;
}

int test_01_is_directory(){
    printf("Running is_directory tests...\n");

    const char *test_dir = "test_dir";
    const char *test_nested = "test_dir/test_subdir";
    const char *test_file = "test_file.txt";

    mkdir(test_dir, 0755);
    mkdir(test_nested, 0755);
    FILE *f = fopen(test_file, "w");
    fclose(f);

    // Test 1: valid directory 
    assert(is_directory(test_dir) == true);
    printf("Test 1 Passed: Valid directory\n");

    // Test 2: Valid File
    assert(is_directory(test_file) == false);
    printf("Test 2 Passed: Valid File\n");

    // Test 3: Nested directory
    assert(is_directory(test_nested) == true);
    printf("Test 3 Passed: Valid nested direcotry\n");

    // Test 4: Non-existent path
    assert(is_directory("src/tmp/tmp") == false);
    printf("Test 4 Passed: Not Valid directory\n");

    // Test 5: Handling NULL
    assert(is_directory(NULL) == false);
    printf("Test 5 Passed: Handling NULL\n");

    rmdir(test_nested);
    rmdir(test_dir);
    remove(test_file);

    printf("\nAll path_join tests passed successfully!\n");

    return EXIT_SUCCESS;
}

int test_02_file_exists() {
    printf("Running file_exists extended tests...\n");

    const char *dir = "test_data";
    const char *nested_dir = "test_data/subdir";
    const char *hidden_file = "test_data/.git_hidden";
    const char *nested_file = "test_data/subdir/config.ini";

    mkdir(dir, 0755);
    mkdir(nested_dir, 0755);
    
    FILE *f1 = fopen(hidden_file, "w");
    if (f1) fclose(f1);
    
    FILE *f2 = fopen(nested_file, "w");
    if (f2) fclose(f2);

    // --- Execution & Assertions ---

    // Test 1: Hidden file existence (dotfile)
    assert(file_exists(hidden_file) == true);
    printf("Test 1 Passed: Found hidden file (.git_hidden)\n");

    // Test 2: Nested file existence
    assert(file_exists(nested_file) == true);
    printf("Test 2 Passed: Found nested file in subdir\n");

    // Test 3: Directory existence (it's a file type too)
    assert(file_exists(nested_dir) == true);
    printf("Test 3 Passed: Found directory as an existing path\n");

    // Test 4: False positive check (non-existent)
    assert(file_exists("test_data/ghost.txt") == false);
    printf("Test 4 Passed: Correctly identified missing file\n");

    // Test 5: NULL path handling
    assert(file_exists(NULL) == false);
    printf("Test 5 Passed: Handled NULL input gracefully\n");

    remove(nested_file);
    remove(hidden_file);
    rmdir(nested_dir);
    rmdir(dir);

    printf("All file_exists tests passed successfully!\n\n");
    return EXIT_SUCCESS;
}

int test_03_mkdir_p() {
    printf("Running mkdir_p tests...\n");

    const char *deep_path = "tmp_a/tmp_b/tmp_c";
    const char *blocking_file = "tmp_block.txt";

    // 1. Test: Create deep nested directory
    assert(mkdir_p(deep_path, 0755) == true);
    struct stat sb;
    assert(stat("tmp_a/tmp_b/tmp_c", &sb) == 0 && S_ISDIR(sb.st_mode));
    printf("Test 1 Passed: Deep nested directory created\n");

    // 2. Test: Run again on existing directory (should be idempotent)
    assert(mkdir_p(deep_path, 0755) == true);
    printf("Test 2 Passed: Idempotency (calling on existing dir succeeds)\n");

    // 3. Test: Path blocked by a file
    FILE *f = fopen(blocking_file, "w");
    fclose(f);
    assert(mkdir_p("tmp_block.txt/subdir", 0755) == false);
    printf("Test 3 Passed: Correctly failed when blocked by a file\n");

    // 4. Test: NULL handling
    assert(mkdir_p(NULL, 0755) == false);
    printf("Test 4 Passed: NULL handled\n");

    rmdir("tmp_a/tmp_b/tmp_c");
    rmdir("tmp_a/tmp_b");
    rmdir("tmp_a");
    remove(blocking_file);

    printf("\nAll mkdir_p tests passed successfully!\n");
    return EXIT_SUCCESS;
}

int test_04_is_directory_empty() {
    printf("Running is_directory_empty tests...\n");

    const char *empty_dir = "test_empty";
    const char *full_dir = "test_full";
    const char *file_in_dir = "test_full/dummy.txt";

    // 1. Setup
    mkdir(empty_dir, 0755);
    mkdir(full_dir, 0755);
    FILE *f = fopen(file_in_dir, "w");
    if (f) fclose(f);


    // Test 1: Genuinely empty directory
    assert(is_directory_empty(empty_dir) == true);
    printf("Test 1 Passed: Correctly identified empty directory\n");

    // Test 2: Directory with a file
    assert(is_directory_empty(full_dir) == false);
    printf("Test 2 Passed: Correctly identified non-empty directory\n");

    // Test 3: Directory with a hidden file (should not be empty)
    const char *hidden_file = "test_empty/.hidden";
    f = fopen(hidden_file, "w");
    if (f) fclose(f);
    assert(is_directory_empty(empty_dir) == false);
    printf("Test 3 Passed: Hidden files make directory non-empty\n");

    // Test 4: Non-existent path
    assert(is_directory_empty("ghost_folder") == false);
    printf("Test 4 Passed: Handled missing path\n");

    // --- Teardown ---
    remove(hidden_file);
    remove(file_in_dir);
    rmdir(empty_dir);
    rmdir(full_dir);

    printf("\nAll is_directory_empty tests passed!\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is right of the following:\n");
        fprintf(stderr, "    0. Test str_join\n");
        fprintf(stderr, "    1. Test is_directory\n");
        fprintf(stderr, "    2. Test file_exists\n");
        fprintf(stderr, "    3. Test mkdir_p\n");
        fprintf(stderr, "    4. Test is_directory_empty\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_path_join(); break;
        case 1:  status = test_01_is_directory(); break;
        case 2:  status = test_02_file_exists(); break;
        case 3:  status = test_03_mkdir_p(); break;
        case 4:  status = test_04_is_directory_empty(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}