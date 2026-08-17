#include "index.h"
#include "repository.h"
#include "utils.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Helpers */

static Repository *create_temp_repo(char *out_path_buf, size_t buf_size) {
    snprintf(out_path_buf, buf_size, "/tmp/test_git_index_XXXXXX");
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

static void write_index_file(Repository *repo, const unsigned char *data, size_t len) {
    char *index_path = repo_file(repo, false, "index", NULL);
    assert(index_path != NULL);
    int fd = open(index_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);
    ssize_t written = write(fd, data, len);
    assert(written == (ssize_t)len);
    close(fd);
    free(index_path);
}

/* Tests */

int test_00_index_read_empty() {
    printf("Running index_read empty index test...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    // Build minimal index file: header (12 bytes) with count=0
    unsigned char raw[12] = {0};
    memcpy(raw, "DIRC", 4);
    raw[4] = 0; raw[5] = 0; raw[6] = 0; raw[7] = 2; // version 2
    raw[8] = 0; raw[9] = 0; raw[10] = 0; raw[11] = 0; // count=0

    write_index_file(repo, raw, sizeof(raw));

    GitIndex *idx = index_read(repo);
    assert(idx != NULL);
    assert(idx->version == 2);
    assert(idx->count == 0);
    assert(idx->entries == NULL);

    index_destroy(idx);
    destroy_temp_repo(repo);

    printf("Test 0 Passed: empty index file correctly parsed\n");
    return EXIT_SUCCESS;
}

int test_01_index_read_one_entry() {
    printf("Running index_read one entry test...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    // Build a valid index file with one entry.
    unsigned char header[12];
    memcpy(header, "DIRC", 4);
    header[4] = 0; header[5] = 0; header[6] = 0; header[7] = 2;
    header[8] = 0; header[9] = 0; header[10] = 0; header[11] = 1; // count = 1

    unsigned char entry[62] = {0};
    // Set some dummy values for timestamps, device, etc.
    entry[0] = 0x00; entry[1] = 0x00; entry[2] = 0x00; entry[3] = 0x01; // ctime_s
    entry[4] = 0x00; entry[5] = 0x00; entry[6] = 0x00; entry[7] = 0x02; // ctime_ns
    entry[8] = 0x00; entry[9] = 0x00; entry[10] = 0x00; entry[11] = 0x03; // mtime_s
    entry[12] = 0x00; entry[13] = 0x00; entry[14] = 0x00; entry[15] = 0x04; // mtime_ns
    entry[16] = 0x00; entry[17] = 0x00; entry[18] = 0x00; entry[19] = 0x05; // dev
    entry[20] = 0x00; entry[21] = 0x00; entry[22] = 0x00; entry[23] = 0x06; // ino
    entry[24] = 0x00; entry[25] = 0x00; // unused

    // mode = (0b1000 << 12) | 0644
    uint16_t mode = (0b1000 << 12) | 0644;
    entry[26] = (mode >> 8) & 0xFF;
    entry[27] = mode & 0xFF;

    // flags: name length = 8, no assume-valid, no extended, stage=0
    uint16_t flags = 8; // only lower 12 bits matter
    entry[60] = (flags >> 8) & 0xFF;
    entry[61] = flags & 0xFF;

    const char *name = "file.txt";
    size_t name_len = strlen(name);
    size_t entry_unpadded = 62 + name_len + 1;
    size_t padded_len = (entry_unpadded + 7) & ~7;

    unsigned char *index_data = malloc(12 + padded_len);
    assert(index_data != NULL);
    memcpy(index_data, header, 12);
    memcpy(index_data + 12, entry, 62);
    memcpy(index_data + 12 + 62, name, name_len);
    index_data[12 + 62 + name_len] = 0;
    memset(index_data + 12 + 62 + name_len + 1, 0, padded_len - (62 + name_len + 1));

    write_index_file(repo, index_data, 12 + padded_len);
    free(index_data);

    GitIndex *idx = index_read(repo);
    assert(idx != NULL);
    assert(idx->version == 2);
    assert(idx->count == 1);
    assert(idx->entries != NULL);
    assert(strcmp(idx->entries[0].name, "file.txt") == 0);
    assert(idx->entries[0].mode_type == 0b1000);
    assert(idx->entries[0].mode_perms == 0644);

    // The SHA should be 40 hexadecimal zeros because we left that field zeroed
    char expected_sha[41];
    memset(expected_sha, '0', 40);
    expected_sha[40] = '\0';
    assert(strcmp(idx->entries[0].sha, expected_sha) == 0);

    index_destroy(idx);
    destroy_temp_repo(repo);

    printf("Test 1 Passed: one-entry index correctly parsed\n");
    return EXIT_SUCCESS;
}

int test_02_index_read_missing() {
    printf("Running index_read missing index test...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    // No index file created
    GitIndex *idx = index_read(repo);
    assert(idx != NULL);
    assert(idx->version == 2);
    assert(idx->count == 0);
    assert(idx->entries == NULL);

    index_destroy(idx);
    destroy_temp_repo(repo);

    printf("Test 2 Passed: missing index returns empty index\n");
    return EXIT_SUCCESS;
}

int test_03_index_read_invalid_signature() {
    printf("Running index_read invalid signature test...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    unsigned char raw[12] = "NOTDIRCxxxx";
    write_index_file(repo, raw, sizeof(raw));

    GitIndex *idx = index_read(repo);
    assert(idx != NULL); // Should return empty index on error
    assert(idx->version == 2);
    assert(idx->count == 0);

    index_destroy(idx);
    destroy_temp_repo(repo);

    printf("Test 3 Passed: invalid signature returns empty index\n");
    return EXIT_SUCCESS;
}

int test_04_index_read_invalid_version() {
    printf("Running index_read invalid version test...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    unsigned char raw[12];
    memcpy(raw, "DIRC", 4);
    raw[4] = 0; raw[5] = 0; raw[6] = 0; raw[7] = 3; // version 3
    raw[8] = 0; raw[9] = 0; raw[10] = 0; raw[11] = 0;

    write_index_file(repo, raw, sizeof(raw));

    GitIndex *idx = index_read(repo);
    assert(idx != NULL);
    assert(idx->version == 2); // default remains 2
    assert(idx->count == 0);

    index_destroy(idx);
    destroy_temp_repo(repo);

    printf("Test 4 Passed: invalid version returns empty index\n");
    return EXIT_SUCCESS;
}

int test_05_index_destroy_null() {
    printf("Running index_destroy NULL test...\n");

    index_destroy(NULL); // must not crash

    printf("Test 5 Passed: index_destroy(NULL) is safe\n");
    return EXIT_SUCCESS;
}

int test_06_index_destroy_with_entries() {
    printf("Running index_destroy with entries test...\n");

    GitIndex *idx = calloc(1, sizeof(GitIndex));
    assert(idx != NULL);
    idx->version = 2;
    idx->count = 2;
    idx->entries = calloc(2, sizeof(IndexEntry));
    assert(idx->entries != NULL);
    idx->entries[0].name = strdup("a.txt");
    idx->entries[1].name = strdup("b.txt");
    assert(idx->entries[0].name && idx->entries[1].name);

    index_destroy(idx); // should free all

    printf("Test 6 Passed: index_destroy frees entries and index\n");
    return EXIT_SUCCESS;
}

int test_07_index_write_roundtrip(void) {
    printf("Running index_write roundtrip test...\n");
    char template[] = "/tmp/git_clone_test_XXXXXX";
    char *repo_path = mkdtemp(template);
    assert(repo_path != NULL);
    Repository *repo = repo_init(repo_path);
    assert(repo != NULL);

    GitIndex *idx = safe_calloc(1, sizeof(GitIndex));
    idx->version = 2;
    idx->entries = safe_calloc(2, sizeof(IndexEntry));
    idx->count = 2;
    idx->capacity = 2;

    /* Entry 0 */
    IndexEntry *e = &idx->entries[0];
    e->name = safe_strdup("file1.txt");
    e->ctime_s = 1234567890; e->ctime_ns = 123;
    e->mtime_s = 1234567891; e->mtime_ns = 456;
    e->dev = 65; e->ino = 1234;
    e->mode_type = 0b1000; e->mode_perms = 0644;
    e->uid = 1000; e->gid = 1000; e->fsize = 12345;
    strcpy(e->sha, "1111111111111111111111111111111111111111");
    e->flag_assume_valid = false; e->flag_stage = 0;

    /* Entry 1 */
    e = &idx->entries[1];
    e->name = safe_strdup("long/path/to/file2.txt");
    e->ctime_s = 1234567892; e->ctime_ns = 789;
    e->mtime_s = 1234567893; e->mtime_ns = 101;
    e->dev = 65; e->ino = 5678;
    e->mode_type = 0b1000; e->mode_perms = 0755;
    e->uid = 1000; e->gid = 1000; e->fsize = 67890;
    strcpy(e->sha, "2222222222222222222222222222222222222222");
    e->flag_assume_valid = false; e->flag_stage = 0;

    bool ok = index_write(repo, idx);
    assert(ok);

    GitIndex *read_idx = index_read(repo);
    assert(read_idx != NULL);
    assert(read_idx->count == idx->count);
    assert(read_idx->version == idx->version);

    for (size_t i = 0; i < idx->count; i++) {
        assert(streq(read_idx->entries[i].name, idx->entries[i].name));
        assert(read_idx->entries[i].ctime_s == idx->entries[i].ctime_s);
        assert(read_idx->entries[i].ctime_ns == idx->entries[i].ctime_ns);
        assert(read_idx->entries[i].mtime_s == idx->entries[i].mtime_s);
        assert(read_idx->entries[i].mtime_ns == idx->entries[i].mtime_ns);
        assert(read_idx->entries[i].dev == idx->entries[i].dev);
        assert(read_idx->entries[i].ino == idx->entries[i].ino);
        assert(read_idx->entries[i].mode_type == idx->entries[i].mode_type);
        assert(read_idx->entries[i].mode_perms == idx->entries[i].mode_perms);
        assert(read_idx->entries[i].uid == idx->entries[i].uid);
        assert(read_idx->entries[i].gid == idx->entries[i].gid);
        assert(read_idx->entries[i].fsize == idx->entries[i].fsize);
        assert(streq(read_idx->entries[i].sha, idx->entries[i].sha));
    }

    index_destroy(idx);
    index_destroy(read_idx);
    repo_destroy(repo);
    remove_directory(repo_path);
    /* repo_path is stack */
    printf("Test 0 Passed: index_write roundtrip preserves all fields\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is one of the following:\n");
        fprintf(stderr, "    0. Test empty index parsing\n");
        fprintf(stderr, "    1. Test one-entry index parsing\n");
        fprintf(stderr, "    2. Test missing index handling\n");
        fprintf(stderr, "    3. Test invalid signature handling\n");
        fprintf(stderr, "    4. Test invalid version handling\n");
        fprintf(stderr, "    5. Test index_destroy with NULL\n");
        fprintf(stderr, "    6. Test index_destroy with entries\n");
        fprintf(stderr, "    7. Test index_write roundtrip\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_index_read_empty(); break;
        case 1:  status = test_01_index_read_one_entry(); break;
        case 2:  status = test_02_index_read_missing(); break;
        case 3:  status = test_03_index_read_invalid_signature(); break;
        case 4:  status = test_04_index_read_invalid_version(); break;
        case 5:  status = test_05_index_destroy_null(); break;
        case 6:  status = test_06_index_destroy_with_entries(); break;
        case 7:  status = test_07_index_write_roundtrip(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}