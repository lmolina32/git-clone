/* index.c: parses the .git/index (staging area) file, format v2 */

#include "index.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

/**
 * read_binary32 - reads a 32-bit big-endian integer from a byte buffer
 *
 * @param p  Pointer to the first of four bytes to read.
 *
 * @return The 32‑bit unsigned integer represented by the bytes.
 **/
static uint32_t read_binary32(const unsigned char *p){
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}

/**
 * read_binary16 - reads a 16-bit big-endian integer from a byte buffer
 *
 * @param p  Pointer to the first of two bytes to read.
 *
 * @return The 16‑bit unsigned integer represented by the bytes.
 **/
static uint16_t read_binary16(const unsigned char *p){
    return ((uint16_t)p[0] << 8) | ((uint16_t)p[1]);
}

/**
 * index_add_entry - appends an IndexEntry to a GitIndex
 *
 * @param idx  Pointer to the GitIndex to which the entry is added.
 * @param e    The IndexEntry to append (must have a valid `name` pointer).
 **/
static void index_add_entry(GitIndex *idx, IndexEntry e){
    if (idx->capacity == idx->count){
        idx->capacity = idx->capacity ? idx->capacity * 2 : 8;
        idx->entries = safe_realloc(idx->entries, idx->capacity * sizeof(IndexEntry));
    }
    idx->entries[idx->count++] = e;
}

/**
 * index_read - reads and parses the .git/index file (format v2)
 *
 * Attempts to locate and parse the staging area index file of a repository.
 * If the file does not exist, an empty GitIndex (version 2) is returned.
 * On any parsing error, the function prints a diagnostic and returns an
 * empty or partially constructed index (never NULL, except on allocation
 * failure). The returned GitIndex must be freed with index_destroy().
 *
 * @param repo  Pointer to the target Repository struct.
 *
 * @return A newly allocated GitIndex, or NULL on memory allocation failure.
 **/
GitIndex *index_read(Repository *repo){
    GitIndex *idx = safe_calloc(sizeof(GitIndex), 1);
    idx->version = 2;

    char *index_file = repo_file(repo, false, "index", NULL);
    if (!index_file || !file_exists(index_file)){ free(index_file); return idx; }

    FILE *f = safe_fopen(index_file, "rb");
    free(index_file);

    struct stat sb;

    if (fstat(fileno(f), &sb) != 0){
        fclose(f);
        return idx;
    }

    size_t file_len = sb.st_size;
    unsigned char *raw = safe_calloc(1, file_len);
    if(fread(raw, sizeof(char), file_len, f) != file_len){
        fprintf(stderr, "index_read: short read on index file\n");
        free(raw);
        fclose(f);
        return idx;
    }
    fclose(f);

    if (file_len < 12 || memcmp(raw, "DIRC", 4) != 0){
        fprintf(stderr, "index_read: invalid index file (bad signature)\n");
        free(raw);
        return idx;
    }

    uint32_t version = read_binary32(raw + 4);
    if (version != 2){
        fprintf(stderr, "index_read: git clone only supports index file version 2\n");
        free(raw);
        return idx; 
    }
    uint32_t count = read_binary32(raw + 8);
    idx->version = version;

    size_t pos = 12;
    for (uint32_t i = 0; i < count; i++){
        if (pos + 62 > file_len){
            fprintf(stderr, "index_read: truncated entry\n");
            break;
        }
        IndexEntry e = {0};
        e.ctime_s  = read_binary32(raw + pos);
        e.ctime_ns = read_binary32(raw + pos + 4);
        e.mtime_s  = read_binary32(raw + pos + 8);
        e.mtime_ns = read_binary32(raw + pos + 12); 
        e.dev      = read_binary32(raw + pos + 16);
        e.ino      = read_binary32(raw + pos + 20);

        uint16_t unused = read_binary16(raw + pos + 24);
        if (unused != 0){
            fprintf(stderr, "index_read: unused word not zero (0x%04x)\n", unused);
            goto error;
        }

        uint16_t mode = read_binary16(raw + pos + 26);
        e.mode_type = (uint16_t)(mode >> 12);
        if (e.mode_type != 0b1000 && e.mode_type != 0b1010 && e.mode_type != 0b1110){
            fprintf(stderr, "index_read: invalid mode type 0x%x\n", e.mode_type);
            goto error;
        }
        e.mode_perms = (uint16_t)(mode & 0x01FF);

        e.uid = read_binary32(raw + pos + 28);
        e.gid = read_binary32(raw + pos + 32);
        e.fsize = read_binary32(raw + pos + 36);

        for (int b = 0; b < 20; b++){
            snprintf(e.sha + b * 2, 3, "%02x", raw[pos + 40 + b]);
        }
        e.sha[40] = '\0';

        uint16_t flags = read_binary16(raw + pos + 60);
        e.flag_assume_valid = (flags & 0x8000) != 0;
        bool flag_extended = (flags & 0x4000) != 0;
        if (flag_extended){
            fprintf(stderr, "index_read: extneded flags currently not supported\n");
        }

        e.flag_stage = (uint16_t)((flags & 0x3000) >> 12);

        uint32_t name_length = flags & 0x0FFF;

        pos += 62;

        size_t name_start = pos;
        size_t name_len;
        if (name_length < 0xFFF){
            if (pos + name_length >= file_len || raw[pos + name_length] != 0x00){
                fprintf(stderr, "index_read: unterminated long filename\n");
                goto error;
            }
            name_len = name_length;
            pos += name_length + 1;
        } else {
            size_t p = pos + 0xFFF;
            while (p < file_len && raw[p] != 0x00) p++;
            if (p == file_len){
                fprintf(stderr, "index_read: unterminated long filename\n");
                goto error;
            }
            name_len = p - name_start;
            pos = p + 1;
        }

        e.name = safe_calloc(name_len + 1, 1);
        memcpy(e.name, raw + name_start, name_len);
        e.name[name_len] = '\0';

        pos = (size_t)(8 * ceil((double)pos / 8.0));

        index_add_entry(idx, e);
        continue;

        error:
        if (e.name) free(e.name);
        break;
    }

    free(raw);
    return idx;
}

/**
 * index_destroy - frees all memory associated with a GitIndex
 *
 * @param index  Pointer to the GitIndex to free, or NULL.
 **/
void index_destroy(GitIndex *index){
    if (!index) { return; }
    for (size_t i = 0; i < index->count; i++){
        free(index->entries[i].name);
    }
    free(index->entries);
    free(index);
}
