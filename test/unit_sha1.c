#include "sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int test_00_sha1_empty() {
    printf("Running sha1 empty string test...\n");

    char out[41];
    // SHA-1 of an empty string "" is da39a3ee5e6b4b0d3255bfef95601890afd80709
    sha1_hex((const unsigned char *)"", 0, out);

    assert(strcmp(out, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0);
    printf("Test 1 Passed: Empty string SHA-1 matches expected hash\n");

    return EXIT_SUCCESS;
}

int test_01_sha1_abc() {
    printf("Running sha1 'abc' test...\n");

    char out[41];
    // SHA-1 of "abc" is a9993e364706816aba3e25717850c26c9cd0d89d
    sha1_hex((const unsigned char *)"abc", 3, out);

    assert(strcmp(out, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
    printf("Test 1 Passed: 'abc' SHA-1 matches expected hash\n");

    return EXIT_SUCCESS;
}

int test_02_sha1_multiblock() {
    printf("Running sha1 multi-block test...\n");

    // A string longer than 64 bytes to trigger multiple blocks and padding
    const char *long_str = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    char out[41];
    
    // SHA-1 of this specific string is 84983e441c3bd26ebaae4aa1f95129e5e54670f1
    sha1_hex((const unsigned char *)long_str, strlen(long_str), out);

    assert(strcmp(out, "84983e441c3bd26ebaae4aa1f95129e5e54670f1") == 0);
    printf("Test 1 Passed: Multi-block SHA-1 matches expected hash\n");

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is one of the following:\n");
        fprintf(stderr, "    0. Test empty string SHA-1\n");
        fprintf(stderr, "    1. Test 'abc' SHA-1\n");
        fprintf(stderr, "    2. Test multi-block SHA-1\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_sha1_empty(); break;
        case 1:  status = test_01_sha1_abc(); break;
        case 2:  status = test_02_sha1_multiblock(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}