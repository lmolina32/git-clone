#include "config.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

int test_00_gitconfig_user_get(void) {
    printf("Running gitconfig_user_get test...\n");
    char template[] = "/tmp/git_clone_test_XXXXXX";
    char *fake_home = mkdtemp(template);
    assert(fake_home != NULL);

    char *config_path = path_join(fake_home, ".gitconfig", NULL);
    FILE *f = fopen(config_path, "w");
    assert(f != NULL);
    fprintf(f, "[user]\n\tname = Test User\n\temail = test@example.com\n");
    fclose(f);

    setenv("HOME", fake_home, 1);
    unsetenv("XDG_CONFIG_HOME");

    char *result = gitconfig_user_get();
    assert(result != NULL);
    assert(streq(result, "Test User <test@example.com>"));

    free(result);
    free(config_path);
    remove_directory(fake_home);
    /* fake_home is stack, do NOT free */
    printf("Test 0 Passed: gitconfig_user_get returns correct user string\n");
    return EXIT_SUCCESS;
}

int test_01_gitconfig_user_get_missing_file(void) {
    printf("Running gitconfig_user_get missing file test...\n");
    char template[] = "/tmp/git_clone_test_XXXXXX";
    char *fake_home = mkdtemp(template);
    assert(fake_home != NULL);

    setenv("HOME", fake_home, 1);
    unsetenv("XDG_CONFIG_HOME");

    char *result = gitconfig_user_get();
    assert(result == NULL);

    remove_directory(fake_home);
    /* fake_home is stack, do NOT free */
    printf("Test 1 Passed: gitconfig_user_get returns NULL when config missing\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "  0. gitconfig_user_get returns user string\n");
        fprintf(stderr, "  1. gitconfig_user_get returns NULL when missing\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;
    switch (number) {
        case 0: status = test_00_gitconfig_user_get(); break;
        case 1: status = test_01_gitconfig_user_get_missing_file(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }
    return status;
}