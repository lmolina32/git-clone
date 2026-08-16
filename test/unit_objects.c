#include "objects.h"
#include "ref.h"
#include "utils.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Helpers */

static Repository *create_temp_repo(char *out_path_buf, size_t buf_size) {
    snprintf(out_path_buf, buf_size, "/tmp/test_git_obj_XXXXXX");
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

/* Tests */

int test_00_type_conversions() {
    printf("Running object type conversion test...\n");

    // Enum to Name
    assert(strcmp(object_type_name(GIT_BLOB), "blob") == 0);
    assert(strcmp(object_type_name(GIT_COMMIT), "commit") == 0);
    assert(strcmp(object_type_name(GIT_TAG), "tag") == 0);
    assert(strcmp(object_type_name(GIT_TREE), "tree") == 0);

    // Name to Enum
    object_type type;
    assert(object_type_from_name("blob", &type) && type == GIT_BLOB);
    assert(object_type_from_name("commit", &type) && type == GIT_COMMIT);
    assert(object_type_from_name("tag", &type) && type == GIT_TAG);
    assert(object_type_from_name("tree", &type) && type == GIT_TREE);

    // Invalid Names
    assert(!object_type_from_name("invalid", &type));
    assert(!object_type_from_name("BLOB", &type));

    printf("Test 0 Passed: Object type conversion functions match expected output\n");
    return EXIT_SUCCESS;
}

int test_01_object_new_and_destroy() {
    printf("Running object allocation and destruction test...\n");

    const char *sample_data = "Hello, World!";
    size_t sample_len = strlen(sample_data);

    Object *obj = object_new(GIT_BLOB, (char *)sample_data, sample_len);
    assert(obj != NULL);
    assert(obj->type == GIT_BLOB);
    assert(obj->size == sample_len);
    assert(obj->data != sample_data); 
    assert(memcmp(obj->data, sample_data, sample_len) == 0);

    object_destroy(obj);
    object_destroy(NULL); 
    printf("Test 1 Passed: Object allocation deep-copies buffer and destruction cleans up\n");
    return EXIT_SUCCESS;
}

int test_02_object_write_no_repo() {
    printf("Running object write (no repo) test...\n");

    const char *data = "test content\n";
    size_t size = strlen(data);

    Object *obj = object_new(GIT_BLOB, (char *)data, size);

    // Standard Git SHA-1 calculation for "blob 13\0test content\n"
    char *sha1 = object_write(obj, NULL);

    assert(sha1 != NULL);
    assert(strlen(sha1) == 40);
    assert(strcmp(sha1, "d670460b4b4aece5915caf5c68d12f560a9fe3e4") == 0);

    free(sha1);
    object_destroy(obj);
    assert(object_write(NULL, NULL) == NULL);

    printf("Test 2 Passed: Pure SHA-1 header and content hashing matches Git standard\n");
    return EXIT_SUCCESS;
}

int test_03_object_read_invalid() {
    printf("Running object read invalid input guards test...\n");

    Repository repo = {0};

    assert(object_read(NULL, "d670460b4b4aece5915caf5c68d12f560a9fe3e4") == NULL);
    assert(object_read(&repo, NULL) == NULL);
    assert(object_read(&repo, "ab") == NULL); // Length < 3
    assert(object_read(&repo, "0000000000000000000000000000000000000000") == NULL);

    printf("Test 3 Passed: Invalid object read parameters safely return NULL\n");
    return EXIT_SUCCESS;
}

int test_04_object_hash_fd() {
    printf("Running object hash from file descriptor test...\n");

    char temp_path[] = "/tmp/git_test_XXXXXX";
    int fd = mkstemp(temp_path);
    assert(fd != -1);

    const char *content = "what is up doc?";
    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    assert(written == (ssize_t)len);
    lseek(fd, 0, SEEK_SET);

    char *sha1 = object_hash(fd, GIT_BLOB, NULL);
    assert(sha1 != NULL);
    assert(strcmp(sha1, "0d53affc5f806cfe2880c7b621094f8b4b920529") == 0);

    close(fd);
    unlink(temp_path);
    free(sha1);

    printf("Test 4 Passed: Hashing file descriptor produces expected Git SHA-1\n");
    return EXIT_SUCCESS;
}

int test_05_object_find_and_cat() {
    printf("Running object find and cat test...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    // Create a blob and a branch "main" pointing to it
    Object *blob = object_new(GIT_BLOB, "test content", 12);
    char *blob_sha = object_write(blob, repo);
    object_destroy(blob);
    ref_create(repo, "refs/heads/main", blob_sha);

    // Resolve "main" -> should return the blob SHA
    char *found = object_find(repo, "main", GIT_ANY_TYPE, false);
    assert(found != NULL);
    assert(strcmp(found, blob_sha) == 0);
    free(found);

    // cat_file on a non‑existent object should return false
    bool res = cat_file(repo, "0000000000000000000000000000000000000000", GIT_BLOB);
    assert(res == false);

    free(blob_sha);
    destroy_temp_repo(repo);

    printf("Test 5 Passed: Object find pass-through and cat_file fallback work\n");
    return EXIT_SUCCESS;
}

int test_06_commit_parse() {
    printf("Running commit_parse test...\n");

    const char *raw_commit = "tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904\n\nInitial commit message";
    
    // 1. Test valid commit object
    Object *commit_obj = object_new(GIT_COMMIT, (char *)raw_commit, strlen(raw_commit));
    KVLM *kvlm = commit_parse(commit_obj);
    
    assert(kvlm != NULL);
    assert(strcmp(kvlm_get(kvlm, "tree"), "4b825dc642cb6eb9a060e54bf8d69288fbee4904") == 0);
    assert(strcmp(kvlm_get(kvlm, NULL), "Initial commit message") == 0);

    // 2. Test invalid object type (Blob instead of Commit)
    Object *blob_obj = object_new(GIT_BLOB, "just some text", 14);
    assert(commit_parse(blob_obj) == NULL);

    // 3. Test NULL handling
    assert(commit_parse(NULL) == NULL);

    // Cleanup
    kvlm_destroy(kvlm);
    object_destroy(commit_obj);
    object_destroy(blob_obj);

    printf("Test 5 Passed: commit_parse handles valid commits and rejects non-commits\n");
    return EXIT_SUCCESS;
}

int test_07_commit_from_kvlm() {
    printf("Running commit_from_kvlm test...\n");

    // 1. Setup a KVLM object
    KVLM *kvlm = kvlm_new();
    kvlm_set(kvlm, "tree", "4b825dc642cb6eb9a060e54bf8d69288fbee4904");
    kvlm_set(kvlm, NULL, "My generated commit");

    // 2. Convert to Object
    Object *obj = commit_from_kvlm(kvlm);
    
    assert(obj != NULL);
    assert(obj->type == GIT_COMMIT); // Type should automatically be GIT_COMMIT
    
    const char *expected_data = 
        "tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904\n"
        "\n"
        "My generated commit";

    assert(obj->size == strlen(expected_data));
    assert(memcmp(obj->data, expected_data, obj->size) == 0);

    // Cleanup
    object_destroy(obj);
    kvlm_destroy(kvlm);

    printf("Test 6 Passed: commit_from_kvlm correctly wraps serialized KVLM into an Object\n");
    return EXIT_SUCCESS;
}

int test_08_object_to_tree_invalid() {
    printf("Running object_to_tree invalid inputs test...\n");

    // Test 1: NULL object
    assert(object_to_tree(NULL) == NULL);

    // Test 2: Non-tree object (e.g., a blob)
    Object obj = {0};
    obj.type = GIT_BLOB; 
    obj.data = "some data";
    obj.size = 9;
    
    assert(object_to_tree(&obj) == NULL);

    printf("Test 0 Passed: Invalid inputs correctly return NULL\n");
    return EXIT_SUCCESS;
}

int test_09_object_to_tree_valid() {
    printf("Running object_to_tree valid input test...\n");

    // Construct raw tree data in memory
    char raw_data[100];
    size_t len = 0;

    const char *entry = "100644 hello.txt";
    memcpy(raw_data + len, entry, strlen(entry) + 1); // +1 for null terminator
    len += strlen(entry) + 1;
    
    // 20-byte dummy SHA1
    const unsigned char sha1[20] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14
    };
    memcpy(raw_data + len, sha1, 20);
    len += 20;

    // Create a mock Object struct for the test
    Object obj;
    obj.type = GIT_TREE;
    obj.data = raw_data;
    obj.size = len;

    Tree *t = object_to_tree(&obj);
    
    assert(t != NULL);
    assert(t->count == 1);
    assert(strcmp(t->entries[0].path, "hello.txt") == 0);
    assert(strcmp(t->entries[0].mode, "100644") == 0);

    tree_destroy(t);

    printf("Test 1 Passed: Valid GIT_TREE object parsed into Tree struct\n");
    return EXIT_SUCCESS;
}

int test_10_tree_to_object_invalid() {
    printf("Running tree_to_object invalid inputs test...\n");

    assert(tree_to_object(NULL) == NULL);

    printf("Test 2 Passed: NULL tree correctly returns NULL\n");
    return EXIT_SUCCESS;
}

int test_11_tree_to_object_valid() {
    printf("Running tree_to_object valid input test...\n");

    Tree *t = tree_new();
    assert(t != NULL);
    
    tree_add_entry(t, "100644", "test.c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

    Object *obj = tree_to_object(t);
    
    assert(obj != NULL);
    assert(obj->type == GIT_TREE);
    assert(obj->size > 0);
    assert(obj->data != NULL);

    // Verify the serialized data contains our path
    // (Since tree_serialize writes the mode and path in plaintext before the null byte)
    assert(strstr((char *)obj->data, "100644 test.c") != NULL);

    // Cleanup
    tree_destroy(t);
    
    // Assuming object_free or similar cleanup exists. If you use a different 
    // function to free objects, update this accordingly (e.g., object_destroy(obj)).
    free(obj->data);
    free(obj);

    printf("Test 3 Passed: Valid Tree struct correctly serialized to Object\n");
    return EXIT_SUCCESS;
}

int test12_object_find_refs_and_head() {
    printf("Running test12_object_find_refs_and_head...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    // Create and write a blob object
    Object *blob = object_new(GIT_BLOB, "hello world\n", 12);
    char *blob_sha = object_write(blob, repo);
    object_destroy(blob);

    // Create branch master pointing to blob SHA
    ref_create(repo, "refs/heads/master", blob_sha);

    // 1. Resolve HEAD reference
    char *found_head = object_find(repo, "HEAD", GIT_ANY_TYPE, false);
    assert(found_head != NULL);
    assert(strcmp(found_head, blob_sha) == 0);
    free(found_head);

    // 2. Resolve short branch reference "master"
    char *found_ref = object_find(repo, "master", GIT_ANY_TYPE, false);
    assert(found_ref != NULL);
    assert(strcmp(found_ref, blob_sha) == 0);
    free(found_ref);

    // 3. Non-existent reference returns NULL
    char *missing = object_find(repo, "nonexistent_branch", GIT_ANY_TYPE, false);
    assert(missing == NULL);

    free(blob_sha);
    destroy_temp_repo(repo);
    printf("Test 12 Passed: object_find resolves HEAD and branch reference names\n");
    return EXIT_SUCCESS;
}

int test13_object_find_hex_prefix() {
    printf("Running test13_object_find_hex_prefix...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    Object *blob = object_new(GIT_BLOB, "prefix test content", 19);
    char *full_sha = object_write(blob, repo);
    object_destroy(blob);

    // 1. Full 40-character hexadecimal SHA string lookup
    char *found_full = object_find(repo, full_sha, GIT_ANY_TYPE, false);
    assert(found_full != NULL);
    assert(strcmp(found_full, full_sha) == 0);
    free(found_full);

    // 2. Short prefix lookup (first 7 characters)
    char prefix[8] = {0};
    strncpy(prefix, full_sha, 7);

    char *found_prefix = object_find(repo, prefix, GIT_ANY_TYPE, false);
    assert(found_prefix != NULL);
    assert(strcmp(found_prefix, full_sha) == 0);
    free(found_prefix);

    free(full_sha);
    destroy_temp_repo(repo);
    printf("Test 13 Passed: object_find resolves full and short hexadecimal SHA prefixes\n");
    return EXIT_SUCCESS;
}

int test14_object_find_type_following() {
    printf("Running test14_object_find_type_following...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    // 1. Create a real tree object so that follow = true can read it.
    Tree *tree = tree_new();
    tree_add_entry(tree, "100644", "hello.txt",
                   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    Object *tree_obj = tree_to_object(tree);
    char *tree_sha = object_write(tree_obj, repo);
    tree_destroy(tree);
    object_destroy(tree_obj);

    // 2. Create a commit pointing to that tree.
    KVLM *commit_kvlm = kvlm_new();
    kvlm_set(commit_kvlm, "tree", tree_sha);
    kvlm_set(commit_kvlm, "author", "Test User <test@example.com>");
    kvlm_set(commit_kvlm, NULL, "Initial commit\n");

    Object *commit_obj = commit_from_kvlm(commit_kvlm);
    char *commit_sha = object_write(commit_obj, repo);
    kvlm_destroy(commit_kvlm);
    object_destroy(commit_obj);

    // Follow commit -> tree lookup
    char *found_tree = object_find(repo, commit_sha, GIT_TREE, true);
    assert(found_tree != NULL);
    assert(strcmp(found_tree, tree_sha) == 0);
    free(found_tree);

    // Without following (follow = false), commit isn't a tree -> returns NULL
    char *no_tree = object_find(repo, commit_sha, GIT_TREE, false);
    assert(no_tree == NULL);

    // 3. Create annotated tag object pointing to commit_sha
    KVLM *tag_kvlm = kvlm_new();
    kvlm_set(tag_kvlm, "object", commit_sha);
    kvlm_set(tag_kvlm, "type", "commit");
    kvlm_set(tag_kvlm, "tag", "v1.0");
    kvlm_set(tag_kvlm, NULL, "Tag message\n");

    Object *tag_obj = object_from_kvlm(GIT_TAG, tag_kvlm);
    char *tag_sha = object_write(tag_obj, repo);
    kvlm_destroy(tag_kvlm);
    object_destroy(tag_obj);

    // Follow tag -> commit lookup
    char *found_commit_from_tag = object_find(repo, tag_sha, GIT_COMMIT, true);
    assert(found_commit_from_tag != NULL);
    assert(strcmp(found_commit_from_tag, commit_sha) == 0);
    free(found_commit_from_tag);

    free(commit_sha);
    free(tag_sha);
    free(tree_sha);
    destroy_temp_repo(repo);

    printf("Test 14 Passed: object_find correctly follows tags and commits to target types\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is one of the following:\n");
        fprintf(stderr, "    0. Test object type conversions\n");
        fprintf(stderr, "    1. Test object allocation and destruction\n");
        fprintf(stderr, "    2. Test object writing without repository\n");
        fprintf(stderr, "    3. Test invalid object read guards\n");
        fprintf(stderr, "    4. Test object hashing from file descriptor\n");
        fprintf(stderr, "    5. Test object find and cat_file helpers\n");
        fprintf(stderr, "    6. Test commit parse\n");
        fprintf(stderr, "    7. Test commit from kvlm\n");
        fprintf(stderr, "    8. Test object_to_tree invalid inputs\n");
        fprintf(stderr, "    9. Test object_to_tree valid input\n");
        fprintf(stderr, "    10. Test tree_to_object invalid inputs\n");
        fprintf(stderr, "    11. Test tree_to_object valid input\n");
        fprintf(stderr, "    12. Test object find refs and head\n");
        fprintf(stderr, "    13. Test object find hex prefix\n");
        fprintf(stderr, "    14. Test object find type following\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_type_conversions(); break;
        case 1:  status = test_01_object_new_and_destroy(); break;
        case 2:  status = test_02_object_write_no_repo(); break;
        case 3:  status = test_03_object_read_invalid(); break;
        case 4:  status = test_04_object_hash_fd(); break;
        case 5:  status = test_05_object_find_and_cat(); break;
        case 6:  status = test_06_commit_parse(); break;
        case 7:  status = test_07_commit_from_kvlm(); break;
        case 8:  status = test_08_object_to_tree_invalid(); break;
        case 9:  status = test_09_object_to_tree_valid(); break;
        case 10: status = test_10_tree_to_object_invalid(); break;
        case 11: status = test_11_tree_to_object_valid(); break;
        case 12: status = test12_object_find_refs_and_head(); break;
        case 13: status = test13_object_find_hex_prefix(); break;
        case 14: status = test14_object_find_type_following(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}