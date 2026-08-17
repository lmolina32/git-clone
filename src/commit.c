/* commit.c: implement commit object creation */

#include "commit.h"
#include "objects.h"
#include "kvlm.h"

/**
 * commit_create - Create a commit object and write it to the object store.
 *
 * Builds a commit from the given tree, parent (if any), author information,
 * timestamp, and message. The commit is serialised via KVLM and stored as an
 * object. The returned SHA‑1 must be freed by the caller.
 *
 * @param repo       Repository pointer.
 * @param tree_sha   SHA‑1 of the root tree (hex string).
 * @param parent_sha SHA‑1 of the parent commit, or NULL for initial commit.
 * @param author     Author string, e.g. "John Doe <john@example.com>".
 * @param timestamp  Unix timestamp (seconds since epoch).
 * @param message    Commit message (will be trimmed and newline‑terminated).
 * @return Newly allocated SHA‑1 hex string, or NULL on failure.
 */
char *commit_create(Repository *repo, const char *tree_sha, const char *parent_sha, 
                    const char *author, time_t timestamp, const char *message){
    KVLM *kvlm = kvlm_new();
    kvlm_set(kvlm, "tree", tree_sha);
    if (parent_sha){ 
        kvlm_set(kvlm, "parent", parent_sha);
    }

    struct tm local_tm;
    localtime_r(&timestamp, &local_tm);
    long offset_sec = local_tm.tm_gmtoff;
    char sign = offset_sec >= 0 ? '+' : '-';
    long abs_off = labs(offset_sec);
    int hours = (int)(abs_off / 3600);
    int minutes = (int)((abs_off % 3600) / 60);

    char author_line[512];
    snprintf(author_line, sizeof(author_line), "%s %ld %c%02d%02d",
            author, (long)timestamp, sign, hours, minutes);

    kvlm_set(kvlm, "author", author_line);
    kvlm_set(kvlm, "committer", author_line);

    char *trimmed = safe_strdup(message);
    size_t len = strlen(trimmed);
    while (len > 0 && (trimmed[len - 1] == ' ' || trimmed[len - 1] == '\n')){
        trimmed[--len] = '\0';
    }
    char *msg_nl = safe_calloc(len + 2, 1);
    sprintf(msg_nl, "%s\n", trimmed);
    free(trimmed);

    kvlm_set(kvlm, NULL, msg_nl);
    free(msg_nl);

    Object *commit_obj = commit_from_kvlm(kvlm);
    char *sha = object_write(commit_obj, repo);

    kvlm_destroy(kvlm);
    object_destroy(commit_obj);
    return sha;
}