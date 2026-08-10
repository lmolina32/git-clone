/* sha1.c: SHA-1 implementation per FIPS 180-1 */

#include "sha1.h"
#include <string.h>
#include <stdio.h>

/* Macros */

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

/* Functions */

static void sha1_process_block(SHA1_CTX *ctx, const unsigned char block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4]     << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8)  |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = ROTL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                    k = 0xCA62C1D6; }

        uint32_t temp = ROTL(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ROTL(b, 30);
        b = a;
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void sha1_init(SHA1_CTX *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
    ctx->buffer_len = 0;
}

void sha1_update(SHA1_CTX *ctx, const unsigned char *data, size_t len) {
    ctx->count += len;

    while (len > 0) {
        size_t take = 64 - ctx->buffer_len;
        if (take > len) take = len;

        memcpy(ctx->buffer + ctx->buffer_len, data, take);
        ctx->buffer_len += take;
        data += take;
        len -= take;

        if (ctx->buffer_len == 64) {
            sha1_process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

void sha1_final(SHA1_CTX *ctx, unsigned char digest[20]) {
    uint64_t bit_count = ctx->count * 8;

    /* append the 0x80 padding byte */
    unsigned char pad = 0x80;
    sha1_update(ctx, &pad, 1);

    /* pad with zeros until buffer_len == 56 (mod 64) */
    unsigned char zero = 0x00;
    while (ctx->buffer_len != 56) {
        sha1_update(ctx, &zero, 1);
    }

    /* append 64-bit big-endian bit length */
    unsigned char len_bytes[8];
    for (int i = 0; i < 8; i++) {
        len_bytes[i] = (unsigned char)(bit_count >> (56 - i * 8));
    }
    /* bypass sha1_update's count tracking for the length field itself */
    memcpy(ctx->buffer + ctx->buffer_len, len_bytes, 8);
    sha1_process_block(ctx, ctx->buffer);

    for (int i = 0; i < 5; i++) {
        digest[i * 4]     = (unsigned char)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)(ctx->state[i]);
    }
}

void sha1_hex(const unsigned char *data, size_t len, char out[41]) {
    SHA1_CTX ctx;
    unsigned char digest[20];

    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, digest);

    for (int i = 0; i < 20; i++) {
        snprintf(out + i * 2, 3, "%02x", digest[i]);
    }
    out[40] = '\0';
}