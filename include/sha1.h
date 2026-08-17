/* sha1.h: computing sha1 of data */

#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <stddef.h>

/* Structures */

typedef struct {
    uint32_t        state[5];
    uint64_t        count;        
    unsigned char   buffer[64];
    size_t          buffer_len;
} SHA1_CTX;

/* Functions */

void sha1_init(SHA1_CTX *ctx);
void sha1_update(SHA1_CTX *ctx, const unsigned char *data, size_t len);
void sha1_final(SHA1_CTX *ctx, unsigned char digest[20]);
void sha1_hex(const unsigned char *data, size_t len, char out[41]);

#endif