/* test_tree.c: unit tests for tree.c */

#include "tree.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int test_00_tree_new_empty() {
    printf("Running tree_new empty test...\n");
    Tree *t = tree_new();
    assert(t != NULL);
    assert(t->count == 0);
    tree_destroy(t);
    printf("Test 0 Passed: tree_new returns an empty tree\n");
    return EXIT_SUCCESS;
}

int test_01_tree_add_entry_pads_short_mode() {
    printf("Running tree_add_entry mode padding test...\n");
    Tree *t = tree_new();

    /* 5-char mode "40000" should be padded to "040000" */
    tree_add_entry(t, "40000", "src", "0000000000000000000000000000000000000a");

    assert(t->count == 1);
    assert(strcmp(t->entries[0].mode, "040000") == 0);
    assert(strcmp(t->entries[0].path, "src") == 0);
    assert(strcmp(t->entries[0].sha, "0000000000000000000000000000000000000a") == 0);

    tree_destroy(t);
    printf("Test 1 Passed: 5-char mode is padded with leading zero\n");
    return EXIT_SUCCESS;
}

int test_02_tree_add_entry_keeps_full_mode() {
    printf("Running tree_add_entry full mode test...\n");
    Tree *t = tree_new();

    tree_add_entry(t, "100644", "main.c", "1111111111111111111111111111111111111b");

    assert(t->count == 1);
    assert(strcmp(t->entries[0].mode, "100644") == 0);

    tree_destroy(t);
    printf("Test 2 Passed: 6-char mode is stored unchanged\n");
    return EXIT_SUCCESS;
}

int test_03_tree_entry_type() {
    printf("Running tree_entry_type test...\n");

    assert(streq(tree_entry_type("040000"), "tree"));
    assert(streq(tree_entry_type("100644"), "blob"));
    assert(streq(tree_entry_type("100755"), "blob"));
    assert(streq(tree_entry_type("120000"), "blob"));   /* symlink */
    assert(streq(tree_entry_type("160000"), "commit")); /* submodule */
    assert(tree_entry_type("999999") == NULL);

    printf("Test 3 Passed: tree_entry_type maps all known modes correctly\n");
    return EXIT_SUCCESS;
}

int test_04_tree_serialize_sorts_entries() {
    printf("Running tree_serialize sort order test...\n");
    Tree *t = tree_new();

    tree_add_entry(t, "100644", "foo.c",  "1111111111111111111111111111111111111a");
    tree_add_entry(t, "040000", "foo",    "2222222222222222222222222222222222222b"); /* dir */
    tree_add_entry(t, "100644", "README", "3333333333333333333333333333333333333c");

    size_t len;
    char *raw = tree_serialize(t, &len);
    assert(raw != NULL);

    Tree *parsed = tree_parse(raw, len);
    assert(parsed != NULL);
    assert(parsed->count == 3);

    assert(streq(parsed->entries[0].path, "README"));
    assert(streq(parsed->entries[1].path, "foo.c"));
    assert(streq(parsed->entries[2].path, "foo"));

    free(raw);
    tree_destroy(t);
    tree_destroy(parsed);
    printf("Test 4 Passed: entries sort with directories treated as trailing-slash names\n");
    return EXIT_SUCCESS;
}

int test_05_tree_parse_serialize_roundtrip() {
    printf("Running tree_parse/tree_serialize round-trip test...\n");
    Tree *t = tree_new();

    tree_add_entry(t, "100644", ".gitignore", "894a44cc066a027465cd26d634948d56d13af9a");
    tree_add_entry(t, "040000", "src",         "6d208e47659a2a10f5f8640e0155d9276a2130a");
    tree_add_entry(t, "100755", "run.sh",       "aafc00f9f92091f141982363e0a16f612819e19");

    size_t len;
    char *raw = tree_serialize(t, &len);
    assert(raw != NULL);
    assert(len > 0);

    Tree *parsed = tree_parse(raw, len);
    assert(parsed != NULL);
    assert(parsed->count == t->count);

    for (size_t i = 0; i < t->count; i++) {
        assert(streq(t->entries[i].mode, parsed->entries[i].mode));
        assert(streq(t->entries[i].path, parsed->entries[i].path));
        assert(streq(t->entries[i].sha,  parsed->entries[i].sha));
    }

    free(raw);
    tree_destroy(t);
    tree_destroy(parsed);
    printf("Test 5 Passed: serialized tree parses back to identical entries\n");
    return EXIT_SUCCESS;
}

int test_06_tree_parse_rejects_malformed_data() {
    printf("Running tree_parse malformed data test...\n");

    /* No space anywhere in the buffer -- mode/path boundary can't be found */
    const char *bad = "not_a_valid_tree_entry_at_all";
    Tree *t = tree_parse(bad, strlen(bad));
    assert(t == NULL);

    printf("Test 6 Passed: malformed tree data is rejected\n");
    return EXIT_SUCCESS;
}

int test_07_tree_parse_empty_buffer() {
    printf("Running tree_parse empty buffer test...\n");

    Tree *t = tree_parse("", 0);
    assert(t != NULL);
    assert(t->count == 0);

    tree_destroy(t);
    printf("Test 7 Passed: an empty payload parses to an empty tree\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is one of the following:\n");
        fprintf(stderr, "    0. Test tree_new returns an empty tree\n");
        fprintf(stderr, "    1. Test tree_add_entry pads a 5-char mode\n");
        fprintf(stderr, "    2. Test tree_add_entry keeps a 6-char mode\n");
        fprintf(stderr, "    3. Test tree_entry_type for all mode prefixes\n");
        fprintf(stderr, "    4. Test tree_serialize sort order (git's dir-as-slash rule)\n");
        fprintf(stderr, "    5. Test tree_parse/tree_serialize round trip\n");
        fprintf(stderr, "    6. Test tree_parse rejects malformed data\n");
        fprintf(stderr, "    7. Test tree_parse on an empty buffer\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0: status = test_00_tree_new_empty(); break;
        case 1: status = test_01_tree_add_entry_pads_short_mode(); break;
        case 2: status = test_02_tree_add_entry_keeps_full_mode(); break;
        case 3: status = test_03_tree_entry_type(); break;
        case 4: status = test_04_tree_serialize_sorts_entries(); break;
        case 5: status = test_05_tree_parse_serialize_roundtrip(); break;
        case 6: status = test_06_tree_parse_rejects_malformed_data(); break;
        case 7: status = test_07_tree_parse_empty_buffer(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}