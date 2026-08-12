#include "kvlm.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_00_kvlm_new_and_destroy() {
    printf("Running kvlm_new and kvlm_destroy test...\n");

    KVLM *kvlm = kvlm_new();
    assert(kvlm != NULL);
    assert(kvlm->count == 0);
    assert(kvlm->capacity == 0);
    assert(kvlm->entries == NULL);

    kvlm_destroy(kvlm);
    
    // Ensure destroying NULL does not cause a segfault
    kvlm_destroy(NULL); 

    printf("Test 0 Passed: KVLM allocation and destruction work correctly\n");
    return EXIT_SUCCESS;
}

int test_01_kvlm_set_and_get() {
    printf("Running kvlm_set and kvlm_get test...\n");

    KVLM *kvlm = kvlm_new();

    // Set standard keys
    kvlm_set(kvlm, "tree", "tree_sha_123");
    kvlm_set(kvlm, "author", "John Doe <john@example.com>");
    
    // Set NULL key (this acts as the commit message)
    kvlm_set(kvlm, NULL, "Initial commit message here.");

    // Validate standard gets
    assert(strcmp(kvlm_get(kvlm, "tree"), "tree_sha_123") == 0);
    assert(strcmp(kvlm_get(kvlm, "author"), "John Doe <john@example.com>") == 0);
    
    // Validate NULL key (message)
    assert(strcmp(kvlm_get(kvlm, NULL), "Initial commit message here.") == 0);
    
    // Validate missing key
    assert(kvlm_get(kvlm, "parent") == NULL);

    kvlm_destroy(kvlm);

    printf("Test 1 Passed: Setting and getting basic keys and the message work\n");
    return EXIT_SUCCESS;
}

int test_02_kvlm_get_all() {
    printf("Running kvlm_get_all (multiple values for one key) test...\n");

    KVLM *kvlm = kvlm_new();

    // Add multiple 'parent' entries (simulating a merge commit)
    kvlm_set(kvlm, "parent", "parent_sha_1");
    kvlm_set(kvlm, "parent", "parent_sha_2");

    size_t count = 0;
    const char *const *parents = kvlm_get_all(kvlm, "parent", &count);
    
    assert(parents != NULL);
    assert(count == 2);
    assert(strcmp(parents[0], "parent_sha_1") == 0);
    assert(strcmp(parents[1], "parent_sha_2") == 0);

    // Test a non-existent key
    size_t missing_count = 0;
    const char *const *missing = kvlm_get_all(kvlm, "missing_key", &missing_count);
    assert(missing == NULL);
    assert(missing_count == 0);

    kvlm_destroy(kvlm);

    printf("Test 2 Passed: Retrieving multiple values for a single key works\n");
    return EXIT_SUCCESS;
}

int test_03_kvlm_parse() {
    printf("Running kvlm_parse (raw string to KVLM struct) test...\n");

    const char *raw_commit = 
        "tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904\n"
        "parent abcdef1234567890abcdef1234567890abcdef12\n"
        "parent 9876543210987654321098765432109876543210\n"
        "author Linus Torvalds <torvalds@linux-foundation.org> 1114615822 -0700\n"
        "\n"
        "This is a multi-line\n"
        "commit message.\n";

    KVLM *kvlm = kvlm_parse(raw_commit, strlen(raw_commit));
    assert(kvlm != NULL);

    // Verify parsed single-value headers
    assert(strcmp(kvlm_get(kvlm, "tree"), "4b825dc642cb6eb9a060e54bf8d69288fbee4904") == 0);
    assert(strcmp(kvlm_get(kvlm, "author"), "Linus Torvalds <torvalds@linux-foundation.org> 1114615822 -0700") == 0);

    // Verify parsed multi-value headers
    size_t parent_count = 0;
    const char *const *parents = kvlm_get_all(kvlm, "parent", &parent_count);
    assert(parent_count == 2);
    assert(strcmp(parents[0], "abcdef1234567890abcdef1234567890abcdef12") == 0);
    assert(strcmp(parents[1], "9876543210987654321098765432109876543210") == 0);

    // Verify message (NULL key)
    assert(strcmp(kvlm_get(kvlm, NULL), "This is a multi-line\ncommit message.\n") == 0);

    kvlm_destroy(kvlm);

    printf("Test 3 Passed: KVLM correctly parses raw Git commit object strings\n");
    return EXIT_SUCCESS;
}

int test_04_kvlm_serialize() {
    printf("Running kvlm_serialize (KVLM struct to raw string) test...\n");

    KVLM *kvlm = kvlm_new();
    kvlm_set(kvlm, "tree", "4b825dc642cb6eb9a060e54bf8d69288fbee4904");
    kvlm_set(kvlm, "parent", "abcdef1234567890abcdef1234567890abcdef12");
    kvlm_set(kvlm, "author", "Test Author <test@example.com>");
    kvlm_set(kvlm, NULL, "My serialized commit message");

    size_t out_len = 0;
    char *serialized = kvlm_serialize(kvlm, &out_len);
    assert(serialized != NULL);

    const char *expected = 
        "tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904\n"
        "parent abcdef1234567890abcdef1234567890abcdef12\n"
        "author Test Author <test@example.com>\n"
        "\n"
        "My serialized commit message";

    assert(out_len == strlen(expected));
    assert(strcmp(serialized, expected) == 0);

    free(serialized);
    kvlm_destroy(kvlm);

    printf("Test 4 Passed: KVLM correctly serializes into a valid Git commit format\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is one of the following:\n");
        fprintf(stderr, "    0. Test kvlm_new and kvlm_destroy\n");
        fprintf(stderr, "    1. Test kvlm_set and kvlm_get (single entries and message)\n");
        fprintf(stderr, "    2. Test kvlm_get_all (multiple entries per key)\n");
        fprintf(stderr, "    3. Test kvlm_parse (parsing raw string into KVLM)\n");
        fprintf(stderr, "    4. Test kvlm_serialize (struct into raw string)\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_kvlm_new_and_destroy(); break;
        case 1:  status = test_01_kvlm_set_and_get(); break;
        case 2:  status = test_02_kvlm_get_all(); break;
        case 3:  status = test_03_kvlm_parse(); break;
        case 4:  status = test_04_kvlm_serialize(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}