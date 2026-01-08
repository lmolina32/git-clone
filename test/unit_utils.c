/* unit_utils.c: unit test utility functions */

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


int test_00_str_join() {
    printf("Running str_join tests...\n");

    // Test 1: Joining two strings
    char *s1 = str_join("Hello", "World", NULL);
    assert(s1 != NULL);
    assert(streq(s1, "Hello/World"));
    printf("Test 1 Passed: Simple join\n");
    free(s1);

    // Test 2: Joining multiple strings (Path-like)
    char *s2 = str_join("/usr", "local", "bin", "git", NULL);
    assert(s2 != NULL);
    assert(streq(s2, "/usr/local/bin/git"));
    printf("Test 2 Passed: Multi-string join\n");
    free(s2);

    // Test 3: Joining with empty strings
    char *s3 = str_join("git", "", "init", NULL);
    assert(s3 != NULL);
    assert(streq(s3, "git/init"));
    printf("Test 3 Passed: Empty string handling\n");
    free(s3);

    // Test 4: Single argument (just the first string)
    char *s4 = str_join("Standalone", NULL);
    assert(s4 != NULL);
    assert(streq(s4, "Standalone"));
    printf("Test 4 Passed: Single string join\n");
    free(s4);

    // Test 5: Check for NULL return on NULL input
    char *s5 = str_join(NULL);
    assert(s5 == NULL);
    printf("Test 5 Passed: NULL safety\n");

    printf("\nAll str_join tests passed successfully!\n");

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is right of the following:\n");
        fprintf(stderr, "    0. Test str_join\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_str_join(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}