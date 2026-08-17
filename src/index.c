/* index.c: parses the .git/index (staging area) file, format v2 */

#include "index.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>

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
        size_t entry_start = pos;
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

        size_t entry_len = pos - entry_start;
        size_t padded_len = ((entry_len + 7) / 8) * 8;
        pos = entry_start + padded_len;

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

/**
 * write_binary32 - Write a 32‑bit integer in big‑endian order.
 *
 * @param f File pointer.
 * @param v Value to write.
 */
static void write_binary32(FILE *f, uint32_t v){
    unsigned char b[4] = {
        (unsigned char)(v >> 24),
        (unsigned char)(v >> 16),
        (unsigned char)(v >> 8),
        (unsigned char)(v)
    };
    fwrite(b, 1, 4, f);
}

/**
 * write_binary16 - Write a 16‑bit integer in big‑endian order.
 *
 * @param f File pointer.
 * @param v Value to write.
 */
static void write_binary16(FILE *f, uint16_t v){
    unsigned char b[2] = {
        (unsigned char)(v >> 8),
        (unsigned char)(v)
    };
    fwrite(b, 1, 2, f);
}

/**
 * index_write - Write the Git index (`.git/index`) back to disk.
 *
 * Serialises the GitIndex structure into the version 2 binary format. The
 * file is overwritten; on success the repository's index is updated.
 *
 * @param repo  Repository pointer.
 * @param index The GitIndex to write.
 * @return true on success, false on error.
 */
bool index_write(Repository *repo, GitIndex *index){
    char *path = repo_file(repo, false, "index", NULL);
    if (!path) return false;

    FILE *f = safe_fopen(path, "wb");
    free(path);

    fwrite("DIRC", 1, 4, f);
    write_binary32(f, index->version);
    write_binary32(f, (uint32_t)index->count);

    for (size_t i = 0; i < index->count; i++){
        IndexEntry *e = &index->entries[i];
        size_t written = 0;

        write_binary32(f, e->ctime_s);   written += 4;
        write_binary32(f, e->ctime_ns);  written += 4;
        write_binary32(f, e->mtime_s);   written += 4;
        write_binary32(f, e->mtime_ns);  written += 4;
        write_binary32(f, e->dev);       written += 4;
        write_binary32(f, e->ino);       written += 4;

        write_binary16(f, 0);            written += 2;
        uint16_t mode = (uint16_t)((e->mode_type << 12) | e->mode_perms);
        write_binary16(f, mode);         written += 2;

        write_binary32(f, e->uid);       written += 4; 
        write_binary32(f, e->gid);       written += 4; 
        write_binary32(f, e->fsize);     written += 4; 

        unsigned char sha_bin[20];
        for (int b = 0; b < 20; b++){
            unsigned int byte;
            sscanf(e->sha + b * 2, "%2x", &byte);
            sha_bin[b] = (unsigned char)byte;
        }
        fwrite(sha_bin, 1, 20, f);       written += 20;

        size_t name_len = strlen(e->name);
        uint16_t stored_name_len = (uint16_t)(name_len >= 0xFFF ? 0xFFF : name_len);
        uint16_t flags = (uint16_t)((e->flag_assume_valid ? 0x8000 : 0) |
                                     e->flag_stage | stored_name_len);
        write_binary16(f, flags);        written += 2;

        fwrite(e->name, 1, name_len, f); written += name_len;
        fputc(0, f);                     written += 1;

        size_t pad = (written % 8 != 0) ? (8 - written % 8) : 0;
        for (size_t p = 0; p < pad; p++){
            fputc(0, f);
        }
    }

    fclose(f);
    return true;
}