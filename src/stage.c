/* stage.c: implement staging (add) and unstaging (remove) of files */

#include "stage.h"
#include "index.h"
#include "objects.h"
#include "compat.h"
#include "utils.h"

#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>

/**
 * stage_remove - Remove files from the index and optionally from the worktree.
 *
 * Removes matching index entries. If delete_files is true, also deletes the
 * physical files from the worktree. If skip_missing is false, fails when a
 * path is not found in the index.
 *
 * Paths may be relative to the worktree root or absolute. Absolute paths must
 * reside inside the worktree; otherwise the function fails. Relative paths are
 * matched directly against index entry names (which are stored relative to the
 * worktree root). If delete_files is true, any input paths not present in the
 * index are still unlinked from the filesystem.
 *
 * @param repo         Repository pointer.
 * @param paths        Array of paths to remove.
 * @param path_count   Number of paths.
 * @param delete_files If true, remove the actual worktree files.
 * @param skip_missing If true, ignore missing index entries.
 * @return true on success, false on error (e.g., a path is outside the worktree,
 *         or a required index entry is missing and skip_missing is false).
 */
bool stage_remove(Repository *repo, const char *paths[], size_t path_count, bool delete_files, bool skip_missing) {
    GitIndex *idx = index_read(repo);

    char *wt_allocated = realpath(repo->worktree, NULL);
    const char *wt = wt_allocated ? wt_allocated : repo->worktree;

    char *worktree_prefix;
    if (asprintf(&worktree_prefix, "%s/", wt) < 0) {
        fprintf(stderr, "stage_remove: out of memory\n");
        free(wt_allocated);
        index_destroy(idx);
        return false;
    }

    /* Build a list of relative paths from the input paths */
    StrList relpaths;
    string_list_init(&relpaths);

    for (size_t i = 0; i < path_count; i++) {
        const char *rel = paths[i];

        /* If the path is absolute, strip the worktree prefix */
        if (paths[i][0] == '/') {
            if (!strneq(paths[i], worktree_prefix, strlen(worktree_prefix)) &&
                !streq(paths[i], wt)) {
                fprintf(stderr, "stage_remove: cannot remove paths outside of worktree: %s\n", paths[i]);
                free(worktree_prefix);
                free(wt_allocated);
                string_list_destroy(&relpaths);
                index_destroy(idx);
                return false;
            }
            rel = paths[i] + strlen(worktree_prefix);
        }

        /* Add a heap-allocated copy, never a stack pointer */
        string_list_add(&relpaths, rel);
    }

    /* Remove matching index entries */
    IndexEntry *kept = safe_calloc(sizeof(IndexEntry) * (idx->count ? idx->count : 1), 1);
    size_t kept_count = 0;
    StrList to_delete;
    string_list_init(&to_delete);

    for (size_t i = 0; i < idx->count; i++) {
        bool matched = false;

        /* Manually search relpaths for this index entry's name */
        for (size_t j = 0; j < relpaths.count; j++) {
            if (streq(relpaths.items[j], idx->entries[i].name)) {
                free(relpaths.items[j]);
                relpaths.items[j] = relpaths.items[relpaths.count - 1];
                relpaths.count--;
                matched = true;
                break;
            }
        }

        if (matched) {
            if (delete_files) {
                char *full = path_join(wt, idx->entries[i].name, NULL);
                string_list_add(&to_delete, full);
            }
            free(idx->entries[i].name);
        } else {
            kept[kept_count++] = idx->entries[i];
        }
    }

    bool ok = true;
    if (relpaths.count > 0 && !skip_missing && !delete_files) {
        fprintf(stderr, "stage_remove: cannot remove paths not in the index (%zu missing)\n", relpaths.count);
        ok = false;
    }

    /* Delete physical files if requested */
    for (size_t i = 0; i < to_delete.count; i++) {
        unlink(to_delete.items[i]);
    }

    /* If delete_files is true, also delete files for paths not in the index */
    if (delete_files) {
        for (size_t j = 0; j < relpaths.count; j++) {
            char *full = path_join(wt, relpaths.items[j], NULL);
            unlink(full);
            free(full);
        }
    }

    /* Replace index entries */
    free(idx->entries);
    idx->entries = kept;
    idx->count = kept_count;

    if (ok) {
        ok = index_write(repo, idx);
    }

    /* Cleanup */
    string_list_destroy(&relpaths);
    string_list_destroy(&to_delete);
    free(worktree_prefix);
    free(wt_allocated);
    index_destroy(idx);
    return ok;
}


/**
 * index_entry_from_stat - Build an IndexEntry from a file on disk.
 *
 * Hashes the file content into a blob object, then fills the entry with
 * metadata from stat(2). The returned entry has a dynamically allocated name.
 *
 * @param repo    Repository pointer.
 * @param abspath Absolute path to the file.
 * @param relpath Path relative to the worktree root.
 * @return A filled IndexEntry structure (caller must free its name).
 */
static IndexEntry index_entry_from_stat(Repository *repo, const char *abspath, const char *realpath){
    IndexEntry e = {0};

    int fd = open(abspath, O_RDONLY);
    char *sha = object_hash(fd, GIT_BLOB, repo);
    close(fd);
    memcpy(e.sha, sha, 41);
    free(sha);

    struct stat sb;
    stat(abspath, &sb);

    e.ctime_s           = (uint32_t)ST_CTIME_SEC(sb);
    e.ctime_ns          = (uint32_t)ST_CTIME_NSEC(sb);
    e.mtime_s           = (uint32_t)ST_MTIME_SEC(sb);
    e.mtime_ns          = (uint32_t)ST_MTIME_NSEC(sb);
    e.dev               = (uint32_t)sb.st_dev;
    e.ino               = (uint32_t)sb.st_ino;
    e.mode_type         = 0b1000;
    e.mode_perms        = 0644;
    e.uid               = sb.st_uid;
    e.gid               = sb.st_gid;
    e.fsize             = (uint32_t)sb.st_size;
    e.flag_assume_valid = false;
    e.flag_stage        = 0;
    e.name              = safe_strdup(realpath);

    return e;
}

/**
 * stage_add - Add files to the index (stage them).
 *
 * Reads the given files, creates blob objects for their contents, and adds
 * new index entries. Existing entries for the same paths are removed first
 * (by calling stage_remove with delete_files=false and skip_missing=true).
 *
 * Paths may be relative to the worktree root or absolute. Absolute paths must
 * reside inside the worktree. Each file is hashed to produce a blob object,
 * and an index entry is created with metadata from stat(2).
 *
 * @param repo       Repository pointer.
 * @param paths      Array of paths (absolute or relative to worktree).
 * @param path_count Number of paths.
 * @return true on success, false on error (e.g., a path is not a regular file,
 *         is outside the worktree, or an I/O error occurs).
 */
bool stage_add(Repository *repo, const char *paths[], size_t path_count){
    if (!stage_remove(repo, paths, path_count, false, true)) return false;
    char *wt_allocated = realpath(repo->worktree, NULL);
const char *wt = wt_allocated ? wt_allocated : repo->worktree;

    char *worktree_prefix;
    if (asprintf(&worktree_prefix, "%s/", wt) < 0){
        fprintf(stderr, "stage_add: out of memory\n");
        free(wt_allocated);
        return false;
    }

    GitIndex *idx = index_read(repo);
    for (size_t i = 0; i < path_count; i++) {
        char *candidate;

        if (paths[i][0] == '/') {
            candidate = safe_strdup(paths[i]);
        } else {
            candidate = path_join(wt, paths[i], NULL);
        }

        char resolved[PATH_MAX];
        if (!realpath(candidate, resolved)) {
            strncpy(resolved, candidate, sizeof(resolved) - 1);
            resolved[sizeof(resolved) - 1] = '\0';
        }
        free(candidate);

        if (!file_exists(resolved) || is_directory(resolved)) {
            fprintf(stderr, "stage_add: not a file or outside the worktree: %s\n", paths[i]);
            free(worktree_prefix);
            free(wt_allocated);
            index_destroy(idx);
            return false;
        }

        if (!strneq(resolved, worktree_prefix, strlen(worktree_prefix))) {
            fprintf(stderr, "stage_add: not a file, or outside the worktree: %s\n", paths[i]);
            free(worktree_prefix);
            free(wt_allocated);
            index_destroy(idx);
            return false;
        }

        const char *relpath = resolved + strlen(worktree_prefix);

        if (idx->count == idx->capacity) {
            idx->capacity = idx->capacity ? idx->capacity * 2 : 32;
            idx->entries = safe_realloc(idx->entries, idx->capacity * sizeof(IndexEntry));
        }
        idx->entries[idx->count++] = index_entry_from_stat(repo, resolved, relpath);
    }

    bool ok = index_write(repo, idx);
    index_destroy(idx);
    free(wt_allocated);
    free(worktree_prefix);
    return ok;
}