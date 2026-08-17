#include "gitignore.h"
#include "repository.h"
#include "index.h"
#include "objects.h"
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
    snprintf(out_path_buf, buf_size, "/tmp/test_git_ignore_XXXXXX");
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

static void write_file(const char *path, const char *content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);
    ssize_t len = strlen(content);
    assert(write(fd, content, len) == len);
    close(fd);
}

static char *create_blob(Repository *repo, const char *data) {
    Object *blob = object_new(GIT_BLOB, (char *)data, strlen(data));
    assert(blob != NULL);
    char *sha = object_write(blob, repo);
    object_destroy(blob);
    return sha;
}

static void write_index_with_entry(Repository *repo, const char *pathname,
                                   const char *sha, uint16_t mode_type,
                                   uint16_t mode_perms) {
    unsigned char raw_sha[20];
    for (int i = 0; i < 20; i++) {
        unsigned int byte;
        sscanf(sha + 2*i, "%2x", &byte);
        raw_sha[i] = (unsigned char)byte;
    }

    size_t name_len = strlen(pathname);
    size_t entry_unpadded = 62 + name_len + 1;
    size_t padded_len = (entry_unpadded + 7) & ~7;
    unsigned char *index_data = calloc(1, 12 + padded_len);
    assert(index_data != NULL);

    memcpy(index_data, "DIRC", 4);
    index_data[4] = 0; index_data[5] = 0; index_data[6] = 0; index_data[7] = 2;
    index_data[8] = 0; index_data[9] = 0; index_data[10] = 0; index_data[11] = 1;

    unsigned char *entry = index_data + 12;
    memset(entry, 0, 62);

    uint16_t mode = (mode_type << 12) | (mode_perms & 0x01FF);
    entry[26] = (mode >> 8) & 0xFF;
    entry[27] = mode & 0xFF;

    uint16_t flags = (name_len < 0xFFF) ? name_len : 0xFFF;
    entry[60] = (flags >> 8) & 0xFF;
    entry[61] = flags & 0xFF;

    memcpy(entry + 40, raw_sha, 20);
    memcpy(entry + 62, pathname, name_len);
    entry[62 + name_len] = 0;

    char *index_path = repo_file(repo, false, "index", NULL);
    assert(index_path != NULL);
    int fd = open(index_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);
    assert(write(fd, index_data, 12 + padded_len) == (ssize_t)(12 + padded_len));
    close(fd);

    free(index_data);
    free(index_path);
}

/* Tests */

int test_00_check_ignore_basic() {
    printf("Running check_ignore basic test...\n");

    // Use heap allocation because gitignore_destroy frees the struct
    GitIgnore *gi = calloc(1, sizeof(GitIgnore));
    assert(gi != NULL);

    RuleSet rs = {0};
    ruleset_add(&rs, "*.o", true);
    ruleset_add(&rs, "keep.o", false);
    gi_add_absolute(gi, rs);

    assert(check_ignore(gi, "file.o") == true);
    assert(check_ignore(gi, "keep.o") == false);
    assert(check_ignore(gi, "file.c") == false);

    gitignore_destroy(gi);
    printf("Test 0 Passed: absolute rules with negation work\n");
    return EXIT_SUCCESS;
}

int test_01_check_ignore_scoped() {
    printf("Running check_ignore scoped rules test...\n");

    GitIgnore *gi = calloc(1, sizeof(GitIgnore));
    assert(gi != NULL);

    // Scoped rule for "src": ignore all *.tmp in that tree
    RuleSet src_rules = {0};
    ruleset_add(&src_rules, "*.tmp", true);
    gi_add_scoped(gi, "src", src_rules);

    // Scoped rule for "src/deep": re-include specifically "src/deep/important.tmp"
    RuleSet deep_rules = {0};
    ruleset_add(&deep_rules, "src/deep/important.tmp", false);
    gi_add_scoped(gi, "src/deep", deep_rules);

    assert(check_ignore(gi, "src/file.tmp") == true);
    assert(check_ignore(gi, "src/deep/file.tmp") == true);
    assert(check_ignore(gi, "src/deep/important.tmp") == false); // overridden by deeper rule
    assert(check_ignore(gi, "other.tmp") == false);

    gitignore_destroy(gi);
    printf("Test 1 Passed: scoped rules with precedence work\n");
    return EXIT_SUCCESS;
}

int test_02_gitignore_read_empty() {
    printf("Running gitignore_read empty repo test...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    GitIgnore *gi = gitignore_read(repo);
    assert(gi != NULL);
    assert(gi->absolute_count == 0);
    assert(gi->scoped_count == 0);
    assert(check_ignore(gi, "anything") == false);

    gitignore_destroy(gi);
    destroy_temp_repo(repo);
    printf("Test 2 Passed: empty repository yields no ignore rules\n");
    return EXIT_SUCCESS;
}

int test_03_gitignore_read_absolute() {
    printf("Running gitignore_read absolute rules test...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    // Use mkdir=true to create .git/info if needed
    char *exclude_path = repo_file(repo, true, "info/exclude", NULL);
    assert(exclude_path != NULL);
    write_file(exclude_path, "*.log\n");
    free(exclude_path);

    GitIgnore *gi = gitignore_read(repo);
    assert(gi != NULL);
    assert(check_ignore(gi, "debug.log") == true);
    assert(check_ignore(gi, "debug.txt") == false);

    gitignore_destroy(gi);
    destroy_temp_repo(repo);
    printf("Test 3 Passed: info/exclude rules are loaded\n");
    return EXIT_SUCCESS;
}

int test_04_gitignore_read_index_scoped() {
    printf("Running gitignore_read index scoped rules test...\n");

    char repo_path[MAX_PATH];
    Repository *repo = create_temp_repo(repo_path, sizeof(repo_path));

    char *blob_sha = create_blob(repo, "*.o\n");
    assert(blob_sha != NULL);

    write_index_with_entry(repo, ".gitignore", blob_sha, 0b1000, 0644);
    free(blob_sha);

    GitIgnore *gi = gitignore_read(repo);
    assert(gi != NULL);
    assert(gi->scoped_count == 1);
    assert(strcmp(gi->scoped[0].dir, "") == 0);
    assert(check_ignore(gi, "foo.o") == true);
    assert(check_ignore(gi, "foo.c") == false);

    gitignore_destroy(gi);
    destroy_temp_repo(repo);
    printf("Test 4 Passed: .gitignore in index is parsed as scoped rule\n");
    return EXIT_SUCCESS;
}

int test_05_gitignore_destroy_null() {
    printf("Running gitignore_destroy NULL safety test...\n");
    gitignore_destroy(NULL);
    printf("Test 5 Passed: gitignore_destroy(NULL) is safe\n");
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is one of the following:\n");
        fprintf(stderr, "    0. Test check_ignore basic\n");
        fprintf(stderr, "    1. Test check_ignore scoped\n");
        fprintf(stderr, "    2. Test gitignore_read empty repo\n");
        fprintf(stderr, "    3. Test gitignore_read absolute rules\n");
        fprintf(stderr, "    4. Test gitignore_read index scoped rules\n");
        fprintf(stderr, "    5. Test gitignore_destroy NULL\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    switch (number) {
        case 0:  status = test_00_check_ignore_basic(); break;
        case 1:  status = test_01_check_ignore_scoped(); break;
        case 2:  status = test_02_gitignore_read_empty(); break;
        case 3:  status = test_03_gitignore_read_absolute(); break;
        case 4:  status = test_04_gitignore_read_index_scoped(); break;
        case 5:  status = test_05_gitignore_destroy_null(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}