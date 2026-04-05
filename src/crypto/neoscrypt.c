/*
 * Copyright (c) 2009 Colin Percival, 2011 ArtForz
 * Copyright (c) 2012 Andrew Moon (floodyberry)
 * Copyright (c) 2012 Samuel Neves <sneves@dei.uc.pt>
 * Copyright (c) 2014-2016 John Doering <ghostlander@phoenixcoin.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>   /* uintptr_t */
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* memcpy, memmove */

#include "neoscrypt.h"

/*
 * Compile-time invariant: `uint` must be exactly 32 bits.
 *
 * The NeoScrypt and BLAKE2s specifications operate on 32-bit words.
 * Every struct layout, rotation count, and serialisation macro in this
 * file assumes sizeof(unsigned int) == 4.  If this assertion fires,
 * replace `uint` with `uint32_t` throughout and rebuild.
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(uint) == 4,
    "uint must be 32 bits on this platform; see comment above");
#elif defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(uint) == 4,
    "uint must be 32 bits on this platform; see comment above");
#else
typedef char neoscrypt_uint_size_check[sizeof(uint) == 4 ? 1 : -1];
#endif


/* ======================================================================== */
/* SHA-256                                                                   */
/* ======================================================================== */

#ifdef SHA256

static const uint sha256_constants[64] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};

#define Ch(x,y,z)  (z ^ (x & (y ^ z)))
#define Maj(x,y,z) (((x | y) & z) | (x & y))
#define S0(x)      (ROTR32(x,  2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define S1(x)      (ROTR32(x,  6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define G0(x)      (ROTR32(x,  7) ^ ROTR32(x, 18) ^ (x >>  3))
#define G1(x)      (ROTR32(x, 17) ^ ROTR32(x, 19) ^ (x >> 10))
#define W0(in,i)   (U8TO32_BE(&in[i * 4]))
#define W1(i)      (G1(w[i - 2]) + w[i - 7] + G0(w[i - 15]) + w[i - 16])
#define STEP(i) \
    t1 = S0(r[0]) + Maj(r[0], r[1], r[2]); \
    t0 = r[7] + S1(r[4]) + Ch(r[4], r[5], r[6]) + sha256_constants[i] + w[i]; \
    r[7] = r[6]; \
    r[6] = r[5]; \
    r[5] = r[4]; \
    r[4] = r[3] + t0; \
    r[3] = r[2]; \
    r[2] = r[1]; \
    r[1] = r[0]; \
    r[0] = t0 + t1;

typedef struct sha256_hash_state_t {
    uint  H[8];
    ullong T;
    uint  leftover;
    uchar buffer[BLOCK_SIZE];
} sha256_hash_state;

static void sha256_blocks(sha256_hash_state *S, const uchar *in, uint blocks) {
    uint r[8], w[64], t0, t1, i;

    for(i = 0; i < 8; i++)
        r[i] = S->H[i];

    while(blocks--) {
        for(i =  0; i < 16; i++) w[i] = W0(in, i);
        for(i = 16; i < 64; i++) w[i] = W1(i);
        for(i =  0; i < 64; i++) { STEP(i); }
        for(i =  0; i <  8; i++) {
            r[i]    += S->H[i];
            S->H[i]  = r[i];
        }
        S->T += BLOCK_SIZE * 8;
        in   += BLOCK_SIZE;
    }
}

static void neoscrypt_hash_init_sha256(sha256_hash_state *S) {
    S->H[0] = 0x6A09E667;
    S->H[1] = 0xBB67AE85;
    S->H[2] = 0x3C6EF372;
    S->H[3] = 0xA54FF53A;
    S->H[4] = 0x510E527F;
    S->H[5] = 0x9B05688C;
    S->H[6] = 0x1F83D9AB;
    S->H[7] = 0x5BE0CD19;
    S->T       = 0;
    S->leftover = 0;
}

static void neoscrypt_hash_update_sha256(sha256_hash_state *S,
  const uchar *in, uint inlen) {
    uint blocks, want;

    /* Handle the previous data */
    if(S->leftover) {
        want = (BLOCK_SIZE - S->leftover);
        want = (want < inlen) ? want : inlen;
        neoscrypt_copy(S->buffer + S->leftover, in, want);
        S->leftover += (uint)want;
        if(S->leftover < BLOCK_SIZE)
            return;
        in    += want;
        inlen -= want;
        sha256_blocks(S, S->buffer, 1);
    }

    /* Handle the current data */
    blocks      = (inlen & ~(BLOCK_SIZE - 1));
    S->leftover = (uint)(inlen - blocks);
    if(blocks) {
        sha256_blocks(S, in, blocks / BLOCK_SIZE);
        in += blocks;
    }

    /* Handle leftover data */
    if(S->leftover)
        neoscrypt_copy(S->buffer, in, S->leftover);
}

static void neoscrypt_hash_finish_sha256(sha256_hash_state *S, uchar *hash) {
    ullong t = S->T + (S->leftover * 8);

    S->buffer[S->leftover] = 0x80;
    if(S->leftover <= 55) {
        neoscrypt_erase(S->buffer + S->leftover + 1, 55 - S->leftover);
    } else {
        neoscrypt_erase(S->buffer + S->leftover + 1, 63 - S->leftover);
        sha256_blocks(S, S->buffer, 1);
        neoscrypt_erase(S->buffer, 56);
    }

    U64TO8_BE(S->buffer + 56, t);
    sha256_blocks(S, S->buffer, 1);

    U32TO8_BE(&hash[ 0], S->H[0]);
    U32TO8_BE(&hash[ 4], S->H[1]);
    U32TO8_BE(&hash[ 8], S->H[2]);
    U32TO8_BE(&hash[12], S->H[3]);
    U32TO8_BE(&hash[16], S->H[4]);
    U32TO8_BE(&hash[20], S->H[5]);
    U32TO8_BE(&hash[24], S->H[6]);
    U32TO8_BE(&hash[28], S->H[7]);
}


/* HMAC for SHA-256 */

typedef struct sha256_hmac_state_t {
    sha256_hash_state inner, outer;
} sha256_hmac_state;

static inline void neoscrypt_hmac_init_sha256(sha256_hmac_state *st,
  const uchar *key, uint keylen) {
    uchar pad[BLOCK_SIZE + DIGEST_SIZE];
    uint *P = (uint *) pad;
    uint i;

    /* Initialise the pad for the inner loop (ipad = 0x36) */
    for(i = 0; i < (BLOCK_SIZE >> 2); i++)
        P[i] = 0x36363636;

    if(keylen <= BLOCK_SIZE) {
        /* XOR the key directly into the pad */
        neoscrypt_xor(pad, key, keylen);
    } else {
        /* Hash the key, then XOR the digest into the pad */
        sha256_hash_state st0;
        neoscrypt_hash_init_sha256(&st0);
        neoscrypt_hash_update_sha256(&st0, key, keylen);
        neoscrypt_hash_finish_sha256(&st0, &pad[BLOCK_SIZE]);
        neoscrypt_xor(&pad[0], &pad[BLOCK_SIZE], DIGEST_SIZE);
    }

    neoscrypt_hash_init_sha256(&st->inner);
    neoscrypt_hash_update_sha256(&st->inner, pad, BLOCK_SIZE);

    /* Re-initialise the pad for the outer loop (opad = 0x5C = ipad ^ 0x6A) */
    for(i = 0; i < (BLOCK_SIZE >> 2); i++)
        P[i] ^= (0x36363636 ^ 0x5C5C5C5C);

    neoscrypt_hash_init_sha256(&st->outer);
    neoscrypt_hash_update_sha256(&st->outer, pad, BLOCK_SIZE);
}

static inline void neoscrypt_hmac_update_sha256(sha256_hmac_state *st,
  const uchar *m, uint mlen) {
    neoscrypt_hash_update_sha256(&st->inner, m, mlen);
}

static inline void neoscrypt_hmac_finish_sha256(sha256_hmac_state *st,
  hash_digest mac) {
    hash_digest innerhash;
    neoscrypt_hash_finish_sha256(&st->inner, innerhash);
    neoscrypt_hash_update_sha256(&st->outer, innerhash, sizeof(innerhash));
    neoscrypt_hash_finish_sha256(&st->outer, mac);
}


/* PBKDF2 for SHA-256 */

void neoscrypt_pbkdf2_sha256(const uchar *password, uint password_len,
  const uchar *salt, uint salt_len, uint N, uchar *output, uint output_len) {
    sha256_hmac_state hmac_pw, hmac_pw_salt, work;
    hash_digest ti, u;
    uchar be[4];
    uint i, j, k, blocks;

    /* hmac(password, ...) */
    neoscrypt_hmac_init_sha256(&hmac_pw, password, password_len);

    /* hmac(password, salt...) */
    hmac_pw_salt = hmac_pw;
    neoscrypt_hmac_update_sha256(&hmac_pw_salt, salt, salt_len);

    blocks = ((uint)output_len + (DIGEST_SIZE - 1)) / DIGEST_SIZE;
    for(i = 1; i <= blocks; i++) {
        /* U1 = hmac(password, salt || be(i)) */
        U32TO8_BE(be, i);
        work = hmac_pw_salt;
        neoscrypt_hmac_update_sha256(&work, be, 4);
        neoscrypt_hmac_finish_sha256(&work, ti);
        neoscrypt_copy(u, ti, sizeof(u));

        /* T[i] = U1 ^ U2 ^ U3... */
        for(j = 0; j < N - 1; j++) {
            work = hmac_pw;
            neoscrypt_hmac_update_sha256(&work, u, DIGEST_SIZE);
            neoscrypt_hmac_finish_sha256(&work, u);
            for(k = 0; k < sizeof(u); k++)
                ti[k] ^= u[k];
        }

        /*
         * Copy at most DIGEST_SIZE bytes; on the final (possibly short)
         * block copy only what remains.  Break immediately to avoid
         * unsigned underflow on output_len -= DIGEST_SIZE.
         */
        if(output_len <= DIGEST_SIZE) {
            neoscrypt_copy(output, ti, output_len);
            break;
        }
        neoscrypt_copy(output, ti, DIGEST_SIZE);
        output     += DIGEST_SIZE;
        output_len -= DIGEST_SIZE;
    }
}

#endif /* SHA256 */


/* ======================================================================== */
/* BLAKE-256                                                                 */
/* ======================================================================== */

#ifdef BLAKE256

/*
 * `static` is required here.  Without it these symbols have external
 * linkage and collide with any other BLAKE-256 implementation in the
 * link unit (e.g. libcrypto), causing silent ODR violations on Linux
 * and duplicate-symbol errors on Windows/MSVC.
 */
static const uchar blake256_sigma[] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
    14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3,
    11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4,
     7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8,
     9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13,
     2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9,
    12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11,
    13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10,
     6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5,
    10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0
};

static const uint blake256_constants[16] = {
    0x243F6A88, 0x85A308D3, 0x13198A2E, 0x03707344,
    0xA4093822, 0x299F31D0, 0x082EFA98, 0xEC4E6C89,
    0x452821E6, 0x38D01377, 0xBE5466CF, 0x34E90C6C,
    0xC0AC29B7, 0xC97C50DD, 0x3F84D5B5, 0xB5470917
};

typedef struct blake256_hash_state_t {
    uint H[8];
    uint T[2];
    uint leftover;
    uchar buffer[BLOCK_SIZE];
} blake256_hash_state;

static void blake256_blocks(blake256_hash_state *S, const uchar *in, uint blocks) {
    const uchar *sigma, *sigma_end = blake256_sigma + (10 * 16);
    uint m[16], v[16], h[8], t[2];
    uint i;

    for(i = 0; i < 8; i++) h[i] = S->H[i];
    for(i = 0; i < 2; i++) t[i] = S->T[i];

    while(blocks--) {
        t[0] += 512;
        t[1] += (t[0] < 512) ? 1 : 0;

        for(i = 0; i <  8; i++) v[i]      = h[i];
        for(i = 0; i <  4; i++) v[i +  8] = blake256_constants[i];
        for(i = 0; i <  2; i++) v[i + 12] = blake256_constants[i + 4] ^ t[0];
        for(i = 0; i <  2; i++) v[i + 14] = blake256_constants[i + 6] ^ t[1];

        for(i = 0; i < 16; i++)
            m[i] = U8TO32_BE(&in[i * 4]);

        in += 64;

#define G(a, b, c, d, e) \
    v[a] += (m[sigma[e + 0]] ^ blake256_constants[sigma[e + 1]]) + v[b]; \
    v[d]  = ROTR32(v[d] ^ v[a], 16); \
    v[c] += v[d]; \
    v[b]  = ROTR32(v[b] ^ v[c], 12); \
    v[a] += (m[sigma[e + 1]] ^ blake256_constants[sigma[e + 0]]) + v[b]; \
    v[d]  = ROTR32(v[d] ^ v[a],  8); \
    v[c] += v[d]; \
    v[b]  = ROTR32(v[b] ^ v[c],  7);

        for(i = 0, sigma = blake256_sigma; i < 14; i++) {
            G( 0,  4,  8, 12,  0);
            G( 1,  5,  9, 13,  2);
            G( 2,  6, 10, 14,  4);
            G( 3,  7, 11, 15,  6);

            G( 0,  5, 10, 15,  8);
            G( 1,  6, 11, 12, 10);
            G( 2,  7,  8, 13, 12);
            G( 3,  4,  9, 14, 14);

            sigma += 16;
            if(sigma == sigma_end)
                sigma = blake256_sigma;
        }

#undef G

        for(i = 0; i < 8; i++)
            h[i] ^= (v[i] ^ v[i + 8]);
    }

    for(i = 0; i < 8; i++) S->H[i] = h[i];
    for(i = 0; i < 2; i++) S->T[i] = t[i];
}

static void neoscrypt_hash_init_blake256(blake256_hash_state *S) {
    /*
     * These are the SHA-256 initial hash values, reused by BLAKE-256.
     * No ULL suffix: the target fields are uint (32-bit).
     */
    S->H[0] = 0x6A09E667;
    S->H[1] = 0xBB67AE85;
    S->H[2] = 0x3C6EF372;
    S->H[3] = 0xA54FF53A;
    S->H[4] = 0x510E527F;
    S->H[5] = 0x9B05688C;
    S->H[6] = 0x1F83D9AB;
    S->H[7] = 0x5BE0CD19;
    S->T[0]   = 0;
    S->T[1]   = 0;
    S->leftover = 0;
}

static void neoscrypt_hash_update_blake256(blake256_hash_state *S,
  const uchar *in, uint inlen) {
    uint blocks, want;

    /* Handle the previous data */
    if(S->leftover) {
        want = (BLOCK_SIZE - S->leftover);
        want = (want < inlen) ? want : inlen;
        neoscrypt_copy(S->buffer + S->leftover, in, want);
        S->leftover += (uint)want;
        if(S->leftover < BLOCK_SIZE)
            return;
        in    += want;
        inlen -= want;
        blake256_blocks(S, S->buffer, 1);
    }

    /* Handle the current data */
    blocks      = (inlen & ~(BLOCK_SIZE - 1));
    S->leftover = (uint)(inlen - blocks);
    if(blocks) {
        blake256_blocks(S, in, blocks / BLOCK_SIZE);
        in += blocks;
    }

    /* Handle leftover data */
    if(S->leftover)
        neoscrypt_copy(S->buffer, in, S->leftover);
}

static void neoscrypt_hash_finish_blake256(blake256_hash_state *S, uchar *hash) {
    uint th, tl, bits;

    bits = (S->leftover << 3);
    tl   = S->T[0] + bits;
    th   = S->T[1];
    if(S->leftover == 0) {
        S->T[0] = (uint)0 - (uint)512;
        S->T[1] = (uint)0 - (uint)1;
    } else if(S->T[0] == 0) {
        S->T[0] = ((uint)0 - (uint)512) + bits;
        S->T[1] = S->T[1] - 1;
    } else {
        S->T[0] -= (512 - bits);
    }

    S->buffer[S->leftover] = 0x80;
    if(S->leftover <= 55) {
        neoscrypt_erase(S->buffer + S->leftover + 1, 55 - S->leftover);
    } else {
        neoscrypt_erase(S->buffer + S->leftover + 1, 63 - S->leftover);
        blake256_blocks(S, S->buffer, 1);
        S->T[0] = (uint)0 - (uint)512;
        S->T[1] = (uint)0 - (uint)1;
        neoscrypt_erase(S->buffer, 56);
    }
    S->buffer[55] |= 1;
    U32TO8_BE(S->buffer + 56, th);
    U32TO8_BE(S->buffer + 60, tl);
    blake256_blocks(S, S->buffer, 1);

    U32TO8_BE(&hash[ 0], S->H[0]);
    U32TO8_BE(&hash[ 4], S->H[1]);
    U32TO8_BE(&hash[ 8], S->H[2]);
    U32TO8_BE(&hash[12], S->H[3]);
    U32TO8_BE(&hash[16], S->H[4]);
    U32TO8_BE(&hash[20], S->H[5]);
    U32TO8_BE(&hash[24], S->H[6]);
    U32TO8_BE(&hash[28], S->H[7]);
}


/* HMAC for BLAKE-256 */

typedef struct blake256_hmac_state_t {
    blake256_hash_state inner;
    blake256_hash_state outer;
} blake256_hmac_state;

static inline void neoscrypt_hmac_init_blake256(blake256_hmac_state *st,
  const uchar *key, uint keylen) {
    uchar pad[BLOCK_SIZE + DIGEST_SIZE];
    uint *P = (uint *) pad;
    uint i;

    /* Initialise the pad for the inner loop (ipad = 0x36) */
    for(i = 0; i < (BLOCK_SIZE >> 2); i++)
        P[i] = 0x36363636;

    if(keylen <= BLOCK_SIZE) {
        neoscrypt_xor(pad, key, keylen);
    } else {
        blake256_hash_state st0;
        neoscrypt_hash_init_blake256(&st0);
        neoscrypt_hash_update_blake256(&st0, key, keylen);
        neoscrypt_hash_finish_blake256(&st0, &pad[BLOCK_SIZE]);
        neoscrypt_xor(&pad[0], &pad[BLOCK_SIZE], DIGEST_SIZE);
    }

    neoscrypt_hash_init_blake256(&st->inner);
    neoscrypt_hash_update_blake256(&st->inner, pad, BLOCK_SIZE);

    /* Re-initialise the pad for the outer loop (opad = 0x5C) */
    for(i = 0; i < (BLOCK_SIZE >> 2); i++)
        P[i] ^= (0x36363636 ^ 0x5C5C5C5C);

    neoscrypt_hash_init_blake256(&st->outer);
    neoscrypt_hash_update_blake256(&st->outer, pad, BLOCK_SIZE);
}

static inline void neoscrypt_hmac_update_blake256(blake256_hmac_state *st,
  const uchar *m, uint mlen) {
    neoscrypt_hash_update_blake256(&st->inner, m, mlen);
}

static inline void neoscrypt_hmac_finish_blake256(blake256_hmac_state *st,
  hash_digest mac) {
    hash_digest innerhash;
    neoscrypt_hash_finish_blake256(&st->inner, innerhash);
    neoscrypt_hash_update_blake256(&st->outer, innerhash, sizeof(innerhash));
    neoscrypt_hash_finish_blake256(&st->outer, mac);
}


/* PBKDF2 for BLAKE-256 */

static void neoscrypt_pbkdf2_blake256(const uchar *password,
  uint password_len, const uchar *salt, uint salt_len, uint N,
  uchar *output, uint output_len) {
    blake256_hmac_state hmac_pw, hmac_pw_salt, work;
    hash_digest ti, u;
    uchar be[4];
    uint i, j, k, blocks;

    /* hmac(password, ...) */
    neoscrypt_hmac_init_blake256(&hmac_pw, password, password_len);

    /* hmac(password, salt...) */
    hmac_pw_salt = hmac_pw;
    neoscrypt_hmac_update_blake256(&hmac_pw_salt, salt, salt_len);

    blocks = ((uint)output_len + (DIGEST_SIZE - 1)) / DIGEST_SIZE;
    for(i = 1; i <= blocks; i++) {
        /* U1 = hmac(password, salt || be(i)) */
        U32TO8_BE(be, i);
        work = hmac_pw_salt;
        neoscrypt_hmac_update_blake256(&work, be, 4);
        neoscrypt_hmac_finish_blake256(&work, ti);
        neoscrypt_copy(u, ti, sizeof(u));

        /* T[i] = U1 ^ U2 ^ U3... */
        for(j = 0; j < N - 1; j++) {
            work = hmac_pw;
            neoscrypt_hmac_update_blake256(&work, u, DIGEST_SIZE);
            neoscrypt_hmac_finish_blake256(&work, u);
            for(k = 0; k < sizeof(u); k++)
                ti[k] ^= u[k];
        }

        /*
         * Copy at most DIGEST_SIZE bytes; on the final (possibly short)
         * block copy only what remains.  Break immediately to avoid
         * unsigned underflow on output_len -= DIGEST_SIZE.
         */
        if(output_len <= DIGEST_SIZE) {
            neoscrypt_copy(output, ti, output_len);
            break;
        }
        neoscrypt_copy(output, ti, DIGEST_SIZE);
        output     += DIGEST_SIZE;
        output_len -= DIGEST_SIZE;
    }
}

#endif /* BLAKE256 */

/* ======================================================================== */
/* NeoScrypt core                                                            */
/* ======================================================================== */

#ifdef ASM

/*
 * When compiled with ASM=1 these primitives are provided by the assembly
 * backend.
 *
 * IMPORTANT: the ASM implementation of neoscrypt_erase MUST use volatile
 * stores (or an equivalent platform-specific secure-erase intrinsic such as
 * explicit_bzero / SecureZeroMemory) so that the compiler cannot eliminate
 * the zeroing of sensitive cryptographic key material as dead stores.
 */
extern void neoscrypt_copy(void *dstp, const void *srcp, uint len);
extern void neoscrypt_erase(void *dstp, uint len);
extern void neoscrypt_xor(void *dstp, const void *srcp, uint len);

#else /* !ASM — portable C implementations */

/* Salsa20, rounds must be a multiple of 2 */
static void neoscrypt_salsa(uint *X, uint rounds) {
    uint x0, x1, x2, x3, x4, x5, x6, x7,
         x8, x9, x10, x11, x12, x13, x14, x15, t;

    x0  = X[0];  x1  = X[1];  x2  = X[2];  x3  = X[3];
    x4  = X[4];  x5  = X[5];  x6  = X[6];  x7  = X[7];
    x8  = X[8];  x9  = X[9];  x10 = X[10]; x11 = X[11];
    x12 = X[12]; x13 = X[13]; x14 = X[14]; x15 = X[15];

#define quarter(a, b, c, d) \
    t = a + d; t = ROTL32(t,  7); b ^= t; \
    t = b + a; t = ROTL32(t,  9); c ^= t; \
    t = c + b; t = ROTL32(t, 13); d ^= t; \
    t = d + c; t = ROTL32(t, 18); a ^= t;

    for(; rounds; rounds -= 2) {
        quarter( x0,  x4,  x8, x12);
        quarter( x5,  x9, x13,  x1);
        quarter(x10, x14,  x2,  x6);
        quarter(x15,  x3,  x7, x11);
        quarter( x0,  x1,  x2,  x3);
        quarter( x5,  x6,  x7,  x4);
        quarter(x10, x11,  x8,  x9);
        quarter(x15, x12, x13, x14);
    }

#undef quarter

    X[0]  += x0;  X[1]  += x1;  X[2]  += x2;  X[3]  += x3;
    X[4]  += x4;  X[5]  += x5;  X[6]  += x6;  X[7]  += x7;
    X[8]  += x8;  X[9]  += x9;  X[10] += x10; X[11] += x11;
    X[12] += x12; X[13] += x13; X[14] += x14; X[15] += x15;
}

/* ChaCha20, rounds must be a multiple of 2 */
static void neoscrypt_chacha(uint *X, uint rounds) {
    uint x0, x1, x2, x3, x4, x5, x6, x7,
         x8, x9, x10, x11, x12, x13, x14, x15, t;

    x0  = X[0];  x1  = X[1];  x2  = X[2];  x3  = X[3];
    x4  = X[4];  x5  = X[5];  x6  = X[6];  x7  = X[7];
    x8  = X[8];  x9  = X[9];  x10 = X[10]; x11 = X[11];
    x12 = X[12]; x13 = X[13]; x14 = X[14]; x15 = X[15];

#define quarter(a,b,c,d) \
    a += b; t = d ^ a; d = ROTL32(t, 16); \
    c += d; t = b ^ c; b = ROTL32(t, 12); \
    a += b; t = d ^ a; d = ROTL32(t,  8); \
    c += d; t = b ^ c; b = ROTL32(t,  7);

    for(; rounds; rounds -= 2) {
        quarter( x0,  x4,  x8, x12);
        quarter( x1,  x5,  x9, x13);
        quarter( x2,  x6, x10, x14);
        quarter( x3,  x7, x11, x15);
        quarter( x0,  x5, x10, x15);
        quarter( x1,  x6, x11, x12);
        quarter( x2,  x7,  x8, x13);
        quarter( x3,  x4,  x9, x14);
    }

#undef quarter

    X[0]  += x0;  X[1]  += x1;  X[2]  += x2;  X[3]  += x3;
    X[4]  += x4;  X[5]  += x5;  X[6]  += x6;  X[7]  += x7;
    X[8]  += x8;  X[9]  += x9;  X[10] += x10; X[11] += x11;
    X[12] += x12; X[13] += x13; X[14] += x14; X[15] += x15;
}

/*
 * neoscrypt_blkcpy - copy `len` bytes from srcp to dstp.
 * len must be a multiple of BLOCK_SIZE (64 bytes).
 * Both pointers must be at least 4-byte aligned.
 *
 * memcpy is used instead of a hand-rolled word loop to avoid strict-aliasing
 * undefined behaviour (C99 §6.5p7).  At -O2 the compiler produces equivalent
 * or better code via auto-vectorisation.
 */
static void neoscrypt_blkcpy(void *dstp, const void *srcp, uint len) {
    memcpy(dstp, srcp, (size_t)len);
}

/*
 * neoscrypt_blkswp - swap `len` bytes between blkAp and blkBp in place.
 * len must be a multiple of BLOCK_SIZE (64 bytes).
 *
 * Uses uchar* to remain C99 strict-aliasing safe on any caller type.
 * Compilers vectorise this pattern automatically at -O2.
 */
static void neoscrypt_blkswp(void *blkAp, void *blkBp, uint len) {
    uchar *blkA = (uchar *)blkAp;
    uchar *blkB = (uchar *)blkBp;
    uchar t;
    uint i;
    for(i = 0; i < len; i++) {
        t       = blkA[i];
        blkA[i] = blkB[i];
        blkB[i] = t;
    }
}

/*
 * neoscrypt_blkxor - XOR `len` bytes from srcp into dstp.
 * len must be a multiple of BLOCK_SIZE (64 bytes).
 *
 * Uses uchar* to remain C99 strict-aliasing safe on any caller type.
 * Compilers vectorise this pattern automatically at -O2.
 */
static void neoscrypt_blkxor(void *dstp, const void *srcp, uint len) {
    const uchar *src = (const uchar *)srcp;
    uchar       *dst = (uchar *)dstp;
    uint i;
    for(i = 0; i < len; i++)
        dst[i] ^= src[i];
}

/*
 * neoscrypt_copy - portable memcpy wrapper.
 *
 * Using memcpy (rather than a hand-rolled size_t* loop) is the only
 * C99-correct way to copy between objects of arbitrary type without
 * invoking strict-aliasing undefined behaviour (C99 §6.5p7).
 */
void neoscrypt_copy(void *dstp, const void *srcp, uint len) {
    memcpy(dstp, srcp, (size_t)len);
}

/*
 * neoscrypt_erase - secure memory zeroing.
 *
 * The volatile-qualified write loop prevents the compiler from eliding
 * the zeroing as a dead store when the buffer is freed immediately after
 * (CWE-14).  Do NOT replace this with plain memset(); optimising compilers
 * are explicitly permitted to remove memset() calls whose result is never
 * read (C11 §7.1.4, Footnote 188).
 *
 * On platforms that provide explicit_bzero() (glibc >= 2.25, OpenBSD) or
 * SecureZeroMemory() (Windows), those may be used instead of this loop.
 */
void neoscrypt_erase(void *dstp, uint len) {
    volatile uchar *dst = (volatile uchar *)dstp;
    uint i;
    for(i = 0; i < len; i++)
        dst[i] = 0;
}

/*
 * neoscrypt_xor - XOR `len` bytes from srcp into dstp.
 *
 * Uses uchar* to remain C99 strict-aliasing safe on any caller type.
 */
void neoscrypt_xor(void *dstp, const void *srcp, uint len) {
    const uchar *src = (const uchar *)srcp;
    uchar       *dst = (uchar *)dstp;
    uint i;
    for(i = 0; i < len; i++)
        dst[i] ^= src[i];
}

#endif /* ASM */


/* ======================================================================== */
/* BLAKE2s                                                                   */
/* ======================================================================== */

/*
 * blake2s_param - BLAKE2s parameter block (RFC 7693 §2.1).
 *
 * The parameter block is exactly 32 bytes and is XOR'd into the first
 * eight words of the IV during initialisation.  The layout below matches
 * the specification byte-for-byte on any compiler that does not insert
 * padding between the first four uchar fields and the following uint field;
 * the _Static_assert below enforces this at compile time.
 */
typedef struct blake2s_param_t {
    uchar digest_length;  /* 0: output length in bytes, 1..32           */
    uchar key_length;     /* 1: key length in bytes, 0..32              */
    uchar fanout;         /* 2: fanout (1 for sequential mode)          */
    uchar depth;          /* 3: maximal depth (1 for sequential mode)   */
    uint  leaf_length;    /* 4: maximal byte length of leaf             */
    uchar node_offset[6]; /* 8: node offset                             */
    uchar node_depth;     /* 14: node depth                             */
    uchar inner_length;   /* 15: inner hash byte length                 */
    uchar salt[8];        /* 16: optional salt                          */
    uchar personal[8];    /* 24: optional personalisation               */
} blake2s_param;         /* total: 32 bytes                             */

/*
 * The BLAKE2s spec mandates a 32-byte parameter block.  If this assertion
 * fires, the compiler has inserted padding (check alignment of `uint
 * leaf_length` at offset 4); use __attribute__((packed)) or rearrange.
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(blake2s_param) == 32,
    "blake2s_param must be exactly 32 bytes per RFC 7693 §2.1");
#elif defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(blake2s_param) == 32,
    "blake2s_param must be exactly 32 bytes per RFC 7693 §2.1");
#else
typedef char blake2s_param_size_check[sizeof(blake2s_param) == 32 ? 1 : -1];
#endif

/*
 * blake2s_state - BLAKE2s compression state (256 bytes).
 *
 * buf and tempbuf are declared as uint arrays (not uchar) for two reasons:
 *
 *   1. Alignment: blake2s_compress() casts these regions to uint* for the
 *      compression step.  Declaring them as uchar[N] would leave their
 *      alignment unspecified (potentially 1-byte), which is UB on platforms
 *      that require 4-byte-aligned uint reads (e.g. strict-alignment
 *      aarch64 configurations).  Declaring them as uint[N] guarantees
 *      at least alignof(uint) == 4 by the C standard.
 *
 *   2. The compress function can then use `S->buf` and `S->tempbuf`
 *      directly as uint* without any cast, eliminating the aliasing UB
 *      that would arise from casting uchar* to uint*.
 *
 * Code that needs byte-level access to these fields (blake2s_update,
 * neoscrypt_blake2s) casts to (uchar *), which is always valid per
 * C99 §6.5p7 — uchar may alias any object type.
 *
 * padding[3] is intentional explicit padding to reach exactly 256 bytes;
 * the _Static_assert below enforces this invariant.
 */

/* Number of uint words per BLAKE2s block (64 bytes / 4 bytes per uint). */
#define BLAKE2S_BLOCKWORDS (BLOCK_SIZE / sizeof(uint))  /* = 16 */

typedef struct blake2s_state_t {
    uint  h[8];                          /*   0..31  : chained hash values    */
    uint  t[2];                          /*  32..39  : message byte counter   */
    uint  f[2];                          /*  40..47  : finalisation flags     */
    uint  buf[2 * BLAKE2S_BLOCKWORDS];   /*  48..175 : two-block input buffer */
    uint  buflen;                        /* 176..179 : fill level, in bytes   */
    uint  padding[3];                    /* 180..191 : explicit struct padding */
    uint  tempbuf[BLAKE2S_BLOCKWORDS];   /* 192..255 : compression work space */
} blake2s_state;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(blake2s_state) == 256,
    "blake2s_state must be exactly 256 bytes; check padding[]");
#elif defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(blake2s_state) == 256,
    "blake2s_state must be exactly 256 bytes; check padding[]");
#else
typedef char blake2s_state_size_check[sizeof(blake2s_state) == 256 ? 1 : -1];
#endif

static const uint blake2s_IV[8] = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
    0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

#ifdef ASM
extern void blake2s_compress(blake2s_state *S);
#else /* !ASM */

/*
 * blake2s_compress - BLAKE2s buffer mixer (compressor).
 *
 * `v` and `m` are taken directly from the uint-typed struct fields —
 * no casts required, and no aliasing issue.
 *
 * This is a fully-unrolled 10-round implementation.  The round constants
 * (BLAKE2_SIGMA) are hard-coded per round to avoid an inner loop and
 * a sigma[] table lookup, matching the reference implementation layout.
 */
static void blake2s_compress(blake2s_state *S) {
    uint *v = S->tempbuf;
    uint *m = S->buf;
    uint t0, t1, t2, t3;

    v[0]  = S->h[0];
    v[1]  = S->h[1];
    v[2]  = S->h[2];
    v[3]  = S->h[3];
    v[4]  = S->h[4];
    v[5]  = S->h[5];
    v[6]  = S->h[6];
    v[7]  = S->h[7];
    v[8]  = blake2s_IV[0];
    v[9]  = blake2s_IV[1];
    v[10] = blake2s_IV[2];
    v[11] = blake2s_IV[3];
    v[12] = S->t[0] ^ blake2s_IV[4];
    v[13] = S->t[1] ^ blake2s_IV[5];
    v[14] = S->f[0] ^ blake2s_IV[6];
    v[15] = S->f[1] ^ blake2s_IV[7];

/* Round 0 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[0];  t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[1];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[2];  t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[3];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[4];  t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[5];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[6];  t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[7];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[8];  t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[9];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[10]; t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[11]; v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[12]; t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[13]; v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[14]; t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[15]; v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

/* Round 1 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[14]; t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[10]; v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[4];  t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[8];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[9];  t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[15]; v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[13]; t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[6];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[1];  t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[12]; v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[0];  t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[2];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[11]; t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[7];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[5];  t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[3];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

/* Round 2 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[11]; t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[8];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[12]; t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[0];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[5];  t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[2];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[15]; t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[13]; v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[10]; t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[14]; v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[3];  t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[6];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[7];  t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[1];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[9];  t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[4];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

/* Round 3 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[7];  t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[9];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[3];  t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[1];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[13]; t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[12]; v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[11]; t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[14]; v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[2];  t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[6];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[5];  t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[10]; v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[4];  t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[0];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[15]; t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[8];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

/* Round 4 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[9];  t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[0];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[5];  t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[7];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[2];  t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[4];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[10]; t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[15]; v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[14]; t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[1];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[11]; t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[12]; v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[6];  t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[8];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[3];  t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[13]; v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

/* Round 5 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[2];  t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[12]; v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[6];  t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[10]; v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[0];  t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[11]; v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[8];  t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[3];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[4];  t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[13]; v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[7];  t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[5];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[15]; t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[14]; v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[1];  t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[9];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

/* Round 6 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[12]; t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[5];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[1];  t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[15]; v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[14]; t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[13]; v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[4];  t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[10]; v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[0];  t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[7];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[6];  t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[3];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[9];  t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[2];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[8];  t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[11]; v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

/* Round 7 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[13]; t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[11]; v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[7];  t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[14]; v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[12]; t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[1];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[3];  t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[9];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[5];  t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[0];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[15]; t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[4];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[8];  t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[6];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[2];  t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[10]; v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

/* Round 8 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[6];  t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[15]; v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[14]; t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[9];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[11]; t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[3];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[0];  t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[8];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[12]; t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[2];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[13]; t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[7];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[1];  t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[4];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[10]; t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[5];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

/* Round 9 */
    t0 = v[0];  t1 = v[4];
    t0 = t0 + t1 + m[10]; t3 = v[12]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[2];  v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    t0 = v[1];  t1 = v[5];
    t0 = t0 + t1 + m[8];  t3 = v[13]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[4];  v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[2];  t1 = v[6];
    t0 = t0 + t1 + m[7];  t3 = v[14]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[6];  v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[3];  t1 = v[7];
    t0 = t0 + t1 + m[1];  t3 = v[15]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[5];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[0];  t1 = v[5];
    t0 = t0 + t1 + m[15]; t3 = v[15]; t2 = v[10];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[11]; v[0] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[15] = t3;
    t2 = t2 + t3;  v[10] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[5] = t1;

    t0 = v[1];  t1 = v[6];
    t0 = t0 + t1 + m[9];  t3 = v[12]; t2 = v[11];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[14]; v[1] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[12] = t3;
    t2 = t2 + t3;  v[11] = t2; t1 = ROTR32(t1 ^ t2, 7);  v[6] = t1;

    t0 = v[2];  t1 = v[7];
    t0 = t0 + t1 + m[3];  t3 = v[13]; t2 = v[8];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[12]; v[2] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[13] = t3;
    t2 = t2 + t3;  v[8] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[7] = t1;

    t0 = v[3];  t1 = v[4];
    t0 = t0 + t1 + m[13]; t3 = v[14]; t2 = v[9];
    t3 = ROTR32(t3 ^ t0, 16); t2 = t2 + t3; t1 = ROTR32(t1 ^ t2, 12);
    t0 = t0 + t1 + m[0];  v[3] = t0;
    t3 = ROTR32(t3 ^ t0, 8);  v[14] = t3;
    t2 = t2 + t3;  v[9] = t2;  t1 = ROTR32(t1 ^ t2, 7);  v[4] = t1;

    S->h[0] ^= v[0] ^ v[8];
    S->h[1] ^= v[1] ^ v[9];
    S->h[2] ^= v[2] ^ v[10];
    S->h[3] ^= v[3] ^ v[11];
    S->h[4] ^= v[4] ^ v[12];
    S->h[5] ^= v[5] ^ v[13];
    S->h[6] ^= v[6] ^ v[14];
    S->h[7] ^= v[7] ^ v[15];
}

#endif /* ASM */
/*
 * blake2s_update - absorb `input_size` bytes of input into state S.
 *
 * S->buf is uint[32] (128 bytes).  All byte-level accesses cast to
 * (uchar *), which is the only type C99 §6.5p7 permits to alias any
 * object — so these casts are safe regardless of what the compiler
 * infers about the provenance of the allocation.
 */
static void blake2s_update(blake2s_state *S, const uchar *input,
  uint input_size) {
    uint left, fill;

    while(input_size > 0) {
        left = S->buflen;
        fill = 2U * BLOCK_SIZE - left;
        if(input_size > fill) {
            /* Fill the buffer, compress, then shift the second block down */
            neoscrypt_copy((uchar *)S->buf + left, input, fill);
            S->buflen += fill;
            S->t[0]   += BLOCK_SIZE;
            blake2s_compress(S);
            /*
             * Move the second block (buf[BLOCK_SIZE..2*BLOCK_SIZE-1]) to
             * buf[0..BLOCK_SIZE-1].  The regions are non-overlapping so
             * memcpy (via neoscrypt_copy) is correct.
             */
            neoscrypt_copy(S->buf, S->buf + BLAKE2S_BLOCKWORDS, BLOCK_SIZE);
            S->buflen  -= BLOCK_SIZE;
            input      += fill;
            input_size -= fill;
        } else {
            neoscrypt_copy((uchar *)S->buf + left, input, input_size);
            S->buflen  += input_size;
            input      += input_size;
            input_size  = 0;
        }
    }
}

/*
 * neoscrypt_blake2s - single-call keyed BLAKE2s.
 *
 * Produces output_size bytes of hash into output.
 * key_size must be <= BLOCK_SIZE (64); keys longer than BLOCK_SIZE are
 * silently truncated to BLOCK_SIZE.
 */
void neoscrypt_blake2s(const void *input, const uint input_size,
  const void *key, const uchar key_size, void *output, const uchar output_size) {
    uchar block[BLOCK_SIZE];
    blake2s_param P[1];
    blake2s_state S[1];

    /* Initialise the parameter block */
    neoscrypt_erase(P, 32);
    P->digest_length = output_size;
    P->key_length    = key_size;
    P->fanout        = 1;
    P->depth         = 1;

    /* Initialise state: h = IV XOR P (RFC 7693 §3.1) */
    neoscrypt_erase(S, 256);
    neoscrypt_copy(S->h, blake2s_IV, 32);
    neoscrypt_xor(S->h, P, 32);

    /* Absorb the key block (zero-padded to BLOCK_SIZE) */
    neoscrypt_erase(block, BLOCK_SIZE);
    {
        uchar klen = (key_size > BLOCK_SIZE) ? (uchar)BLOCK_SIZE : key_size;
        neoscrypt_copy(block, key, klen);
    }
    blake2s_update(S, block, BLOCK_SIZE);

    /* Absorb the message */
    blake2s_update(S, (const uchar *)input, input_size);

    /* Finalise */
    if(S->buflen > BLOCK_SIZE) {
        S->t[0] += BLOCK_SIZE;
        blake2s_compress(S);
        S->buflen -= BLOCK_SIZE;
        neoscrypt_copy(S->buf, S->buf + BLAKE2S_BLOCKWORDS, S->buflen);
    }
    S->t[0] += S->buflen;
    S->f[0]  = ~0U;
    neoscrypt_erase((uchar *)S->buf + S->buflen, 2U * BLOCK_SIZE - S->buflen);
    blake2s_compress(S);

    /* Write the first output_size bytes of h[] as the digest */
    neoscrypt_copy(output, S->h, output_size);
}


/* ======================================================================== */
/* FastKDF (reference / non-optimised)                                       */
/* ======================================================================== */

#ifndef OPT

#define FASTKDF_BUFFER_SIZE 256U

/*
 * neoscrypt_fastkdf - FastKDF with BLAKE2s as PRF.
 *
 * FASTKDF_BUFFER_SIZE must be a power of 2.
 * password_len, salt_len, and output_len must not exceed FASTKDF_BUFFER_SIZE.
 * prf_output_size must be <= prf_key_size.
 *
 * Stack layout (relative to 64-byte-aligned base A):
 *
 *   A[   0..255]: password buffer (kdf_buf_size = 256 bytes circular)
 *   A[ 256..319]: extended PRF input window (prf_input_size = 64 bytes)
 *   B[ 320..575]: salt buffer   [B = A + 320]
 *   B[ 256..287]: extended PRF key window (prf_key_size = 32 bytes)
 *   prf_output[608..639]: PRF output workspace (prf_output_size = 32 bytes)
 */
void neoscrypt_fastkdf(const uchar *password, uint password_len,
  const uchar *salt, uint salt_len, uint N, uchar *output, uint output_len) {
    const uintptr_t stack_align = 0x40;
    const uint kdf_buf_size = FASTKDF_BUFFER_SIZE,
      prf_input_size = 64, prf_key_size = 32, prf_output_size = 32;
    uint bufptr, a, b, i, j;
    uchar *A, *B, *prf_input, *prf_key, *prf_output;
    uchar *stack;

    const size_t stack_size = (size_t)(2U * kdf_buf_size + prf_input_size
      + prf_key_size + prf_output_size) + (size_t)stack_align;
    stack = (uchar *)malloc(stack_size);
    if(!stack) {
        neoscrypt_erase(output, output_len);
        return;
    }

    A          = (uchar *)(((uintptr_t)stack & ~(stack_align - 1U)) + stack_align);
    B          = &A[kdf_buf_size + prf_input_size];
    prf_output = &A[2U * kdf_buf_size + prf_input_size + prf_key_size];

    /* Initialise the password buffer */
    if(password_len > kdf_buf_size) password_len = kdf_buf_size;
    if(password_len == 0) {
        free(stack);
        neoscrypt_erase(output, output_len);
        return;
    }

    a = kdf_buf_size / password_len;
    for(i = 0; i < a; i++)
        neoscrypt_copy(&A[i * password_len], &password[0], password_len);
    b = kdf_buf_size - a * password_len;
    if(b)
        neoscrypt_copy(&A[a * password_len], &password[0], b);
    neoscrypt_copy(&A[kdf_buf_size], &password[0], prf_input_size);

    /* Initialise the salt buffer */
    if(salt_len > kdf_buf_size) salt_len = kdf_buf_size;
    if(salt_len == 0) {
        free(stack);
        neoscrypt_erase(output, output_len);
        return;
    }

    a = kdf_buf_size / salt_len;
    for(i = 0; i < a; i++)
        neoscrypt_copy(&B[i * salt_len], &salt[0], salt_len);
    b = kdf_buf_size - a * salt_len;
    if(b)
        neoscrypt_copy(&B[a * salt_len], &salt[0], b);
    neoscrypt_copy(&B[kdf_buf_size], &salt[0], prf_key_size);

    /* Primary iteration */
    for(i = 0, bufptr = 0; i < N; i++) {
        prf_input = &A[bufptr];
        prf_key   = &B[bufptr];

        neoscrypt_blake2s(prf_input, prf_input_size, prf_key, prf_key_size,
          prf_output, prf_output_size);

        for(j = 0, bufptr = 0; j < prf_output_size; j++)
            bufptr += prf_output[j];
        bufptr &= (kdf_buf_size - 1U);

        neoscrypt_xor(&B[bufptr], &prf_output[0], prf_output_size);

        if(bufptr < prf_key_size)
            neoscrypt_copy(&B[kdf_buf_size + bufptr], &B[bufptr],
              MIN(prf_output_size, prf_key_size - bufptr));
        else if((kdf_buf_size - bufptr) < prf_output_size)
            neoscrypt_copy(&B[0], &B[kdf_buf_size],
              prf_output_size - (kdf_buf_size - bufptr));
    }

    if(output_len > kdf_buf_size) output_len = kdf_buf_size;

    a = kdf_buf_size - bufptr;
    if(a >= output_len) {
        neoscrypt_xor(&B[bufptr], &A[0], output_len);
        neoscrypt_copy(&output[0], &B[bufptr], output_len);
    } else {
        neoscrypt_xor(&B[bufptr], &A[0], a);
        neoscrypt_xor(&B[0], &A[a], output_len - a);
        neoscrypt_copy(&output[0], &B[bufptr], a);
        neoscrypt_copy(&output[a], &B[0], output_len - a);
    }

    free(stack);
}

#else /* OPT */

#ifdef ASM

extern void neoscrypt_fastkdf_opt(const uchar *password, const uchar *salt,
  uchar *output, uint mode);

#else /* OPT && !ASM */

/*
 * Initialisation vector with a parameter block XOR'd in.
 *
 * Encodes: digest_length=32, key_length=32, fanout=1, depth=1.
 * Word 0: blake2s_IV[0] ^ 0x01010020 = 0x6A09E667 ^ 0x01010020 = 0x6B08C647
 * All other words are identical to blake2s_IV[].
 */
static const uint blake2s_IV_P_XOR[8] = {
    0x6B08C647, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
    0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

/*
 * neoscrypt_fastkdf_opt - performance-optimised FastKDF with BLAKE2s
 * integrated (no intermediate malloc for the PRF state).
 *
 * Stack layout (relative to 64-byte-aligned base A, 864 bytes total):
 *
 *   Offset   Size   Region
 *   ------   ----   ------
 *        0    320   A: password buffer (256 circular + 64 extended)
 *      320    288   B: salt buffer (256 circular + 32 extended key)
 *      608    256   S: blake2s_state (256 bytes, per _Static_assert)
 *
 * S is addressed via blake2s_state* so field accesses are typed and the
 * compiler can reason about aliasing correctly (C99 §6.3.2.3 permits
 * conversion between object pointer types through void*).
 */
void neoscrypt_fastkdf_opt(const uchar *password, const uchar *salt,
  uchar *output, uint mode) {
    const uintptr_t stack_align = 0x40;
    uint bufptr, output_len, i, j;
    uchar *A, *B;
    blake2s_state *S;
    uchar *stack;

    const size_t stack_size = 864U + (size_t)stack_align;
    stack = (uchar *)malloc(stack_size);
    if(!stack) {
        neoscrypt_erase(output, mode ? 32U : 256U);
        return;
    }

    A = (uchar *)(((uintptr_t)stack & ~(stack_align - 1U)) + stack_align);
    B = &A[320];
    S = (blake2s_state *)(void *)&A[608];

    neoscrypt_copy(&A[0],   &password[0], 80);
    neoscrypt_copy(&A[80],  &password[0], 80);
    neoscrypt_copy(&A[160], &password[0], 80);
    neoscrypt_copy(&A[240], &password[0], 16);
    neoscrypt_copy(&A[256], &password[0], 64);

    if(!mode) {
        output_len = 256;
        neoscrypt_copy(&B[0],   &salt[0], 80);
        neoscrypt_copy(&B[80],  &salt[0], 80);
        neoscrypt_copy(&B[160], &salt[0], 80);
        neoscrypt_copy(&B[240], &salt[0], 16);
        neoscrypt_copy(&B[256], &salt[0], 32);
    } else {
        output_len = 32;
        neoscrypt_copy(&B[0],   &salt[0], 256);
        neoscrypt_copy(&B[256], &salt[0], 32);
    }

    for(i = 0, bufptr = 0; i < 32; i++) {

        /* BLAKE2s: initialise h = IV_P_XOR, zero t/f */
        neoscrypt_copy(S->h, blake2s_IV_P_XOR, 32);
        neoscrypt_erase(S->t, 16);  /* zeros t[2] and f[2] = 16 bytes */

        /* Load key block into buf[0..7], zero buf[8..15] */
        neoscrypt_copy(S->buf, &B[bufptr], 32);
        neoscrypt_erase(S->buf + 8, 32);

        /* Compress with key as first block (t[0] = 64) */
        S->t[0] = 64;
        blake2s_compress(S);

        /* Load input block into buf[0..15] */
        neoscrypt_copy(S->buf, &A[bufptr], 64);

        /* Final compress (t[0] = 128, f[0] = ~0) */
        S->t[0] = 128;
        S->f[0] = ~0U;
        blake2s_compress(S);

        /* Derive next bufptr from all bytes of the 32-byte hash output */
        for(j = 0, bufptr = 0; j < 8; j++) {
            bufptr += S->h[j];
            bufptr += (S->h[j] >> 8);
            bufptr += (S->h[j] >> 16);
            bufptr += (S->h[j] >> 24);
        }
        bufptr &= 0xFF;

        /* XOR hash output into the salt buffer at the new position */
        neoscrypt_xor(&B[bufptr], S->h, 32);

        /* Maintain circular wrap-around shadow copy */
        if(bufptr < 32)
            neoscrypt_copy(&B[256 + bufptr], &B[bufptr], 32 - bufptr);
        else if(bufptr > 224)
            neoscrypt_copy(&B[0], &B[256], bufptr - 224);
    }

    i = 256 - bufptr;
    if(i >= output_len) {
        neoscrypt_xor(&B[bufptr], &A[0], output_len);
        neoscrypt_copy(&output[0], &B[bufptr], output_len);
    } else {
        neoscrypt_xor(&B[bufptr], &A[0], i);
        neoscrypt_xor(&B[0], &A[i], output_len - i);
        neoscrypt_copy(&output[0], &B[bufptr], i);
        neoscrypt_copy(&output[i], &B[0], output_len - i);
    }

    free(stack);
}

#endif /* ASM */

#endif /* OPT */


/* ======================================================================== */
/* NeoScrypt block mixer and core engine                                     */
/* ======================================================================== */

#ifndef ASM

/*
 * neoscrypt_blkmix - configurable optimised block mixer.
 *
 * NeoScrypt flow (r=2, mixer=ChaCha20/20):
 *   Xa ^= Xd; M(Xa'); Ya = Xa";  Xb ^= Xa"; M(Xb'); Yb = Xb";
 *   Xc ^= Xb"; M(Xc'); Yc = Xc"; Xd ^= Xc"; M(Xd'); Yd = Xd";
 *   Xa" = Ya; Xb" = Yc; Xc" = Yb; Xd" = Yd;
 *
 * Scrypt flow (r=1, mixer=Salsa20/8):
 *   Xa ^= Xb; M(Xa'); Ya = Xa"; Xb ^= Xa"; M(Xb'); Yb = Xb";
 *   Xa" = Ya; Xb" = Yb;
 *
 * Y is only accessed in the generic r > 2 path.  For GoByte (r=2) Y is
 * allocated by the caller but not used here; this is intentional and
 * matches the original reference behaviour.
 */
static void neoscrypt_blkmix(uint *X, uint *Y, uint r, uint mixmode) {
    uint i, mixer, rounds;

    mixer  = mixmode >> 8;
    rounds = mixmode & 0xFF;

    if(r == 1) {
        if(mixer) {
            neoscrypt_blkxor(&X[0],  &X[16], BLOCK_SIZE);
            neoscrypt_chacha(&X[0],  rounds);
            neoscrypt_blkxor(&X[16], &X[0],  BLOCK_SIZE);
            neoscrypt_chacha(&X[16], rounds);
        } else {
            neoscrypt_blkxor(&X[0],  &X[16], BLOCK_SIZE);
            neoscrypt_salsa(&X[0],   rounds);
            neoscrypt_blkxor(&X[16], &X[0],  BLOCK_SIZE);
            neoscrypt_salsa(&X[16],  rounds);
        }
        return;
    }

    if(r == 2) {
        if(mixer) {
            neoscrypt_blkxor(&X[0],  &X[48], BLOCK_SIZE);
            neoscrypt_chacha(&X[0],  rounds);
            neoscrypt_blkxor(&X[16], &X[0],  BLOCK_SIZE);
            neoscrypt_chacha(&X[16], rounds);
            neoscrypt_blkxor(&X[32], &X[16], BLOCK_SIZE);
            neoscrypt_chacha(&X[32], rounds);
            neoscrypt_blkxor(&X[48], &X[32], BLOCK_SIZE);
            neoscrypt_chacha(&X[48], rounds);
            neoscrypt_blkswp(&X[16], &X[32], BLOCK_SIZE);
        } else {
            neoscrypt_blkxor(&X[0],  &X[48], BLOCK_SIZE);
            neoscrypt_salsa(&X[0],   rounds);
            neoscrypt_blkxor(&X[16], &X[0],  BLOCK_SIZE);
            neoscrypt_salsa(&X[16],  rounds);
            neoscrypt_blkxor(&X[32], &X[16], BLOCK_SIZE);
            neoscrypt_salsa(&X[32],  rounds);
            neoscrypt_blkxor(&X[48], &X[32], BLOCK_SIZE);
            neoscrypt_salsa(&X[48],  rounds);
            neoscrypt_blkswp(&X[16], &X[32], BLOCK_SIZE);
        }
        return;
    }

    /* Reference path for any r > 2 */
    for(i = 0; i < 2 * r; i++) {
        if(i) neoscrypt_blkxor(&X[16 * i], &X[16 * (i - 1)], BLOCK_SIZE);
        else  neoscrypt_blkxor(&X[0], &X[16 * (2 * r - 1)], BLOCK_SIZE);
        if(mixer)
            neoscrypt_chacha(&X[16 * i], rounds);
        else
            neoscrypt_salsa(&X[16 * i], rounds);
        neoscrypt_blkcpy(&Y[16 * i], &X[16 * i], BLOCK_SIZE);
    }
    for(i = 0; i < r; i++)
        neoscrypt_blkcpy(&X[16 * i],       &Y[16 * 2 * i],       BLOCK_SIZE);
    for(i = 0; i < r; i++)
        neoscrypt_blkcpy(&X[16 * (i + r)], &Y[16 * (2 * i + 1)], BLOCK_SIZE);
}


/*
 * neoscrypt - NeoScrypt proof-of-work hash function.
 *
 * profile bit  0   : 0 = NeoScrypt(128,2,1) Salsa20/20+ChaCha20/20 (GoByte)
 *                    1 = Scrypt(1024,1,1) Salsa20/8
 * profile bits 4-1 : KDF selector (0=FastKDF-BLAKE2s, 1=PBKDF2-SHA256,
 *                                   2=PBKDF2-BLAKE256)
 * profile bit  31  : 1 = extended customisation present
 * profile bits 7-5 : rfactor  (r = 1 << rfactor, max 128)
 * profile bits 12-8: Nfactor  (N = 2 ^ (Nfactor+1), max 2^31)
 * profile bits 30-13: reserved
 *
 * On any failure (OOM, invalid parameters) output is zeroed before
 * returning to prevent the caller from propagating uninitialised data
 * into block validation logic.
 */
void neoscrypt(const uchar *password, uchar *output, uint profile) {
    const uintptr_t stack_align = 0x40;
    uint N = 128, r = 2, dblmix = 1, mixmode = 0x14;
    uint kdf, i, j;
    uint *X, *Y, *Z, *V;
    size_t stack_size;
    uchar *stack;

    if(profile & 0x1) {
        N       = 1024;
        r       = 1;
        dblmix  = 0;
        mixmode = 0x08;
    } else if(profile >> 31) {
        uint nfactor = (profile >>  8) & 0x1F;
        uint rfactor = (profile >>  5) & 0x7;

        N = (nfactor < 31) ? (1U << (nfactor + 1)) : 0x80000000U;
        r = (rfactor <  7) ? (1U << rfactor)        : 128U;
        if(N < 2) N = 2;
        if(r < 1) r = 1;
    }

    /*
     * Guard against parameter combinations that overflow size_t before
     * any multiplication.  N and r are independently bounded first, then
     * the product is capped at 2 GiB to prevent OOM on 32-bit builds.
     */
    if(N > 0x800000U || r > 128U) {
        neoscrypt_erase(output, 32);
        return;
    }
    stack_size = (size_t)(N + 3U) * (size_t)r * 2U * BLOCK_SIZE
               + (size_t)stack_align;
    if(stack_size > 0x80000000U) {
        neoscrypt_erase(output, 32);
        return;
    }

    stack = (uchar *)malloc(stack_size);
    if(!stack) {
        neoscrypt_erase(output, 32);
        return;
    }

    X = (uint *)(((uintptr_t)stack & ~(stack_align - 1U)) + stack_align);
    Z = &X[32U * r];          /* Copy of X for the ChaCha pass             */
    Y = &X[64U * r];          /* Temporary work space (r * 2 * BLOCK_SIZE) */
    V = &X[96U * r];          /* Scratch table: N * r * 2 * BLOCK_SIZE     */

    /* X = KDF(password, salt=password) */
    kdf = (profile >> 1) & 0xF;

    switch(kdf) {
        case 0x0:
        default:
            /*
             * Default (and GoByte's only) path: FastKDF-BLAKE2s.
             * KDF values 3-15 are not defined by the NeoScrypt spec;
             * falling through to FastKDF matches the reference behaviour.
             */
#ifdef OPT
            neoscrypt_fastkdf_opt(password, password, (uchar *)X, 0);
#else
            neoscrypt_fastkdf(password, 80, password, 80, 32,
              (uchar *)X, r * 2U * BLOCK_SIZE);
#endif
            break;

#ifdef SHA256
        case 0x1:
            neoscrypt_pbkdf2_sha256(password, 80, password, 80, 1,
              (uchar *)X, r * 2U * BLOCK_SIZE);
            break;
#endif

#ifdef BLAKE256
        case 0x2:
            neoscrypt_pbkdf2_blake256(password, 80, password, 80, 1,
              (uchar *)X, r * 2U * BLOCK_SIZE);
            break;
#endif
    }

    /*
     * NeoScrypt: run ChaCha SMix first, Salsa SMix second, then XOR.
     * Scrypt:    run Salsa SMix only.
     */
    if(dblmix) {
        /* Z = blkcpy(X) */
        neoscrypt_blkcpy(&Z[0], &X[0], r * 2U * BLOCK_SIZE);

        /* Z = SMix_ChaCha(Z) — fill phase */
        for(i = 0; i < N; i++) {
            neoscrypt_blkcpy(&V[i * (32U * r)], &Z[0], r * 2U * BLOCK_SIZE);
            neoscrypt_blkmix(&Z[0], &Y[0], r, (mixmode | 0x0100));
        }
        /* Z = SMix_ChaCha(Z) — mix phase */
        for(i = 0; i < N; i++) {
            j = (32U * r) * (Z[16U * (2U * r - 1U)] & (N - 1U));
            neoscrypt_blkxor(&Z[0], &V[j], r * 2U * BLOCK_SIZE);
            neoscrypt_blkmix(&Z[0], &Y[0], r, (mixmode | 0x0100));
        }
    }

    /* X = SMix_Salsa(X) — fill phase */
    for(i = 0; i < N; i++) {
        neoscrypt_blkcpy(&V[i * (32U * r)], &X[0], r * 2U * BLOCK_SIZE);
        neoscrypt_blkmix(&X[0], &Y[0], r, mixmode);
    }
    /* X = SMix_Salsa(X) — mix phase */
    for(i = 0; i < N; i++) {
        j = (32U * r) * (X[16U * (2U * r - 1U)] & (N - 1U));
        neoscrypt_blkxor(&X[0], &V[j], r * 2U * BLOCK_SIZE);
        neoscrypt_blkmix(&X[0], &Y[0], r, mixmode);
    }

    if(dblmix)
        neoscrypt_blkxor(&X[0], &Z[0], r * 2U * BLOCK_SIZE);

    /* output = KDF(password, X) */
    switch(kdf) {
        case 0x0:
        default:
#ifdef OPT
            neoscrypt_fastkdf_opt(password, (uchar *)X, output, 1);
#else
            neoscrypt_fastkdf(password, 80, (uchar *)X,
              r * 2U * BLOCK_SIZE, 32, output, 32);
#endif
            break;

#ifdef SHA256
        case 0x1:
            neoscrypt_pbkdf2_sha256(password, 80, (uchar *)X,
              r * 2U * BLOCK_SIZE, 1, output, 32);
            break;
#endif

#ifdef BLAKE256
        case 0x2:
            neoscrypt_pbkdf2_blake256(password, 80, (uchar *)X,
              r * 2U * BLOCK_SIZE, 1, output, 32);
            break;
#endif
    }

    free(stack);
}

#endif /* !ASM */


/* ======================================================================== */
/* 4-way parallel miner (requires ASM=1 and MINER_4WAY=1)                   */
/* NOT part of the consensus-critical node build.                            */
/* ======================================================================== */

#if defined(ASM) && defined(MINER_4WAY)

extern void neoscrypt_xor_salsa_4way(uint *X, uint *X0, uint *Y,
  uint double_rounds);
extern void neoscrypt_xor_chacha_4way(uint *Z, uint *Z0, uint *Y,
  uint double_rounds);

#if (1)

extern void neoscrypt_blkcpy(void *dstp, const void *srcp, uint len);
extern void neoscrypt_blkswp(void *blkAp, void *blkBp, uint len);
extern void neoscrypt_blkxor(void *dstp, const void *srcp, uint len);

extern void neoscrypt_pack_4way(void *dstp, const void *srcp, uint len);
extern void neoscrypt_unpack_4way(void *dstp, const void *srcp, uint len);
extern void neoscrypt_xor_4way(void *dstp, const void *srcAp,
  const void *srcBp, const void *srcCp, const void *srcDp, uint len);

#else /* Reference implementations — not compiled, for documentation only */

static void neoscrypt_pack_4way(void *dstp, const void *srcp, uint len) {
    uint *dst = (uint *)dstp;
    uint *src = (uint *)srcp;
    uint i, j;
    len >>= 4;
    for(i = 0, j = 0; j < len; i += 4, j++) {
        dst[i]     = src[j];
        dst[i + 1] = src[j + len];
        dst[i + 2] = src[j + 2 * len];
        dst[i + 3] = src[j + 3 * len];
    }
}

static void neoscrypt_unpack_4way(void *dstp, const void *srcp, uint len) {
    uint *dst = (uint *)dstp;
    uint *src = (uint *)srcp;
    uint i, j;
    len >>= 4;
    for(i = 0, j = 0; j < len; i += 4, j++) {
        dst[j]           = src[i];
        dst[j + len]     = src[i + 1];
        dst[j + 2 * len] = src[i + 2];
        dst[j + 3 * len] = src[i + 3];
    }
}

static void neoscrypt_xor_4way(void *dstp, const void *srcAp,
  const void *srcBp, const void *srcCp, const void *srcDp, uint len) {
    uint *dst  = (uint *)dstp;
    uint *srcA = (uint *)srcAp;
    uint *srcB = (uint *)srcBp;
    uint *srcC = (uint *)srcCp;
    uint *srcD = (uint *)srcDp;
    uint i;
    for(i = 0; i < (len >> 2); i += 4) {
        dst[i]     ^= srcA[i];
        dst[i + 1] ^= srcB[i + 1];
        dst[i + 2] ^= srcC[i + 2];
        dst[i + 3] ^= srcD[i + 3];
    }
}

#endif /* reference implementations */


/* 4-way NeoScrypt(128, 2, 1) with Salsa20/20 and ChaCha20/20 */
void neoscrypt_4way(const uchar *password, uchar *output, uchar *scratchpad) {
    const uint N = 128, r = 2, double_rounds = 10;
    uint *X, *Z, *V, *Y, *P;
    uint i, j0, j1, j2, j3, k;

    /* Scratchpad: 4 * ((N + 3) * r * 128 + 80) bytes */
    X = (uint *) &scratchpad[0];
    Z = &X[4 * 32 * r];
    V = &X[4 * 64 * r];
    Y = &X[4 * (N + 2) * 32 * r];
    P = &X[4 * (N + 3) * 32 * r];

    for(k = 0; k < 4; k++) {
        neoscrypt_copy(&P[k * 20], password, 80);
        P[(k + 1) * 20 - 1] += k;
    }

    neoscrypt_fastkdf_4way((uchar *)&P[0], (uchar *)&P[0],
      (uchar *)&Y[0], (uchar *)&scratchpad[0], 0);
    neoscrypt_pack_4way(&X[0], &Y[0], 4 * r * 128);
    neoscrypt_blkcpy(&Z[0], &X[0], 4 * r * 128);

    for(i = 0; i < N; i++) {
        neoscrypt_blkcpy(&V[i * r * 128], &Z[0], 4 * r * 128);
        neoscrypt_xor_chacha_4way(&Z[0],   &Z[192], &Y[0], double_rounds);
        neoscrypt_xor_chacha_4way(&Z[64],  &Z[0],   &Y[0], double_rounds);
        neoscrypt_xor_chacha_4way(&Z[128], &Z[64],  &Y[0], double_rounds);
        neoscrypt_xor_chacha_4way(&Z[192], &Z[128], &Y[0], double_rounds);
        neoscrypt_blkswp(&Z[64], &Z[128], r * 128);
    }

    for(i = 0; i < N; i++) {
        j0 = (4 * r * 32) * (Z[64 * (2 * r - 1)    ] & (N - 1));
        j1 = (4 * r * 32) * (Z[64 * (2 * r - 1) + 1] & (N - 1));
        j2 = (4 * r * 32) * (Z[64 * (2 * r - 1) + 2] & (N - 1));
        j3 = (4 * r * 32) * (Z[64 * (2 * r - 1) + 3] & (N - 1));
        neoscrypt_xor_4way(&Z[0], &V[j0], &V[j1], &V[j2], &V[j3], 4 * r * 128);
        neoscrypt_xor_chacha_4way(&Z[0],   &Z[192], &Y[0], double_rounds);
        neoscrypt_xor_chacha_4way(&Z[64],  &Z[0],   &Y[0], double_rounds);
        neoscrypt_xor_chacha_4way(&Z[128], &Z[64],  &Y[0], double_rounds);
        neoscrypt_xor_chacha_4way(&Z[192], &Z[128], &Y[0], double_rounds);
        neoscrypt_blkswp(&Z[64], &Z[128], 256);
    }

    for(i = 0; i < N; i++) {
        neoscrypt_blkcpy(&V[i * r * 128], &X[0], 4 * r * 128);
        neoscrypt_xor_salsa_4way(&X[0],   &X[192], &Y[0], double_rounds);
        neoscrypt_xor_salsa_4way(&X[64],  &X[0],   &Y[0], double_rounds);
        neoscrypt_xor_salsa_4way(&X[128], &X[64],  &Y[0], double_rounds);
        neoscrypt_xor_salsa_4way(&X[192], &X[128], &Y[0], double_rounds);
        neoscrypt_blkswp(&X[64], &X[128], r * 128);
    }

    for(i = 0; i < N; i++) {
        j0 = (4 * r * 32) * (X[64 * (2 * r - 1)    ] & (N - 1));
        j1 = (4 * r * 32) * (X[64 * (2 * r - 1) + 1] & (N - 1));
        j2 = (4 * r * 32) * (X[64 * (2 * r - 1) + 2] & (N - 1));
        j3 = (4 * r * 32) * (X[64 * (2 * r - 1) + 3] & (N - 1));
        neoscrypt_xor_4way(&X[0], &V[j0], &V[j1], &V[j2], &V[j3], 4 * r * 128);
        neoscrypt_xor_salsa_4way(&X[0],   &X[192], &Y[0], double_rounds);
        neoscrypt_xor_salsa_4way(&X[64],  &X[0],   &Y[0], double_rounds);
        neoscrypt_xor_salsa_4way(&X[128], &X[64],  &Y[0], double_rounds);
        neoscrypt_xor_salsa_4way(&X[192], &X[128], &Y[0], double_rounds);
        neoscrypt_blkswp(&X[64], &X[128], r * 128);
    }

    neoscrypt_blkxor(&X[0], &Z[0], 4 * r * 128);
    neoscrypt_unpack_4way(&Y[0], &X[0], 4 * r * 128);

    neoscrypt_fastkdf_4way((uchar *)&P[0], (uchar *)&Y[0],
      (uchar *)&output[0], (uchar *)&scratchpad[0], 1);
}

#ifdef SHA256
/* 4-way Scrypt(1024, 1, 1) with Salsa20/8 */
void scrypt_4way(const uchar *password, uchar *output, uchar *scratchpad) {
    const uint N = 1024, r = 1, double_rounds = 4;
    uint *X, *V, *Y, *P;
    uint i, j0, j1, j2, j3, k;

    /* Scratchpad: 4 * ((N + 2) * r * 128 + 80) bytes */
    X = (uint *) &scratchpad[0];
    V = &X[4 * 32 * r];
    Y = &X[4 * (N + 1) * 32 * r];
    P = &X[4 * (N + 2) * 32 * r];

    for(k = 0; k < 4; k++) {
        neoscrypt_copy(&P[k * 20], password, 80);
        P[(k + 1) * 20 - 1] += k;
    }

    for(k = 0; k < 4; k++)
        neoscrypt_pbkdf2_sha256((uchar *)&P[k * 20], 80,
          (uchar *)&P[k * 20], 80, 1,
          (uchar *)&Y[k * r * 32], r * 128);

    neoscrypt_pack_4way(&X[0], &Y[0], 4 * r * 128);

    for(i = 0; i < N; i++) {
        neoscrypt_blkcpy(&V[i * r * 128], &X[0], 4 * r * 128);
        neoscrypt_xor_salsa_4way(&X[0],  &X[64], &Y[0], double_rounds);
        neoscrypt_xor_salsa_4way(&X[64], &X[0],  &Y[0], double_rounds);
    }

    for(i = 0; i < N; i++) {
        j0 = (4 * r * 32) * (X[64 * (2 * r - 1)    ] & (N - 1));
        j1 = (4 * r * 32) * (X[64 * (2 * r - 1) + 1] & (N - 1));
        j2 = (4 * r * 32) * (X[64 * (2 * r - 1) + 2] & (N - 1));
        j3 = (4 * r * 32) * (X[64 * (2 * r - 1) + 3] & (N - 1));
        neoscrypt_xor_4way(&X[0], &V[j0], &V[j1], &V[j2], &V[j3], 4 * r * 128);
        neoscrypt_xor_salsa_4way(&X[0],  &X[64], &Y[0], double_rounds);
        neoscrypt_xor_salsa_4way(&X[64], &X[0],  &Y[0], double_rounds);
    }

    neoscrypt_unpack_4way(&Y[0], &X[0], 4 * r * 128);

    for(k = 0; k < 4; k++)
        neoscrypt_pbkdf2_sha256((uchar *)&P[k * 20], 80,
          (uchar *)&Y[k * r * 32], r * 128, 1,
          (uchar *)&output[k * 32], 32);
}
#endif /* SHA256 */


extern void blake2s_compress_4way(void *T);

/*
 * 4-way initialisation vector with parameter block (digest_length=32,
 * key_length=32, fanout=1, depth=1) XOR'd in, replicated across 4 lanes.
 *
 * Word 0: blake2s_IV[0] ^ 0x01010020 = 0x6A09E667 ^ 0x01010020 = 0x6B08C647
 * All other words are identical to blake2s_IV[].
 */
static const uint blake2s_IV_P_XOR_4way[32] = {
    0x6B08C647, 0x6B08C647, 0x6B08C647, 0x6B08C647,
    0xBB67AE85, 0xBB67AE85, 0xBB67AE85, 0xBB67AE85,
    0x3C6EF372, 0x3C6EF372, 0x3C6EF372, 0x3C6EF372,
    0xA54FF53A, 0xA54FF53A, 0xA54FF53A, 0xA54FF53A,
    0x510E527F, 0x510E527F, 0x510E527F, 0x510E527F,
    0x9B05688C, 0x9B05688C, 0x9B05688C, 0x9B05688C,
    0x1F83D9AB, 0x1F83D9AB, 0x1F83D9AB, 0x1F83D9AB,
    0x5BE0CD19, 0x5BE0CD19, 0x5BE0CD19, 0x5BE0CD19
};

/*
 * neoscrypt_blake2s_4way - 4-way parallel BLAKE2s.
 *
 * Processes four independent (input[64], key[32]) pairs simultaneously
 * using the 4-way assembly compressor blake2s_compress_4way.
 *
 * Stack layout (relative to 64-byte-aligned base T, uint[176] = 704 bytes):
 *
 *   T[  0.. 31]: 4-way h[]  (8 words * 4 lanes)
 *   T[ 32.. 47]: 4-way t[]/f[] counters
 *   T[ 48.. 79]: 4-way key block (8 words * 4 lanes)
 *   T[ 80..111]: zero padding (buf[32..63] of key pass)
 *   T[112..175]: 4-way input block (16 words * 4 lanes)
 */
void neoscrypt_blake2s_4way(const uchar *input, const uchar *key, uchar *output) {
    const uintptr_t stack_align = 0x40;
    uint *T;
    uchar *stack;

    const size_t stack_size = 704U + (size_t)stack_align;
    stack = (uchar *)malloc(stack_size);
    if(!stack) return;
    T = (uint *)(((uintptr_t)stack & ~(stack_align - 1U)) + stack_align);

    neoscrypt_copy(&T[0],  blake2s_IV_P_XOR_4way, 128);
    neoscrypt_erase(&T[32], 64);

    neoscrypt_pack_4way(&T[48], &key[0], 128);
    neoscrypt_erase(&T[80], 128);

    T[32] = 64; T[33] = 64; T[34] = 64; T[35] = 64;
    blake2s_compress_4way(&T[0]);

    neoscrypt_pack_4way(&T[48], &input[0], 256);

    T[32] = 128; T[33] = 128; T[34] = 128; T[35] = 128;
    T[40] = ~0U; T[41] = ~0U; T[42] = ~0U; T[43] = ~0U;
    blake2s_compress_4way(&T[0]);

    neoscrypt_unpack_4way(&output[0], &T[0], 128);

    free(stack);
}


/*
 * neoscrypt_fastkdf_4way - 4-way parallel FastKDF with BLAKE2s.
 *
 * Scratchpad layout (uint offsets from base T):
 *
 *   T[  0..175]: 4-way BLAKE2s interleaved state workspace
 *   Aa[176..255]: password buffer lane 0 (80 uint = 320 bytes)
 *   Ab[256..335]: password buffer lane 1
 *   Ac[336..415]: password buffer lane 2
 *   Ad[416..495]: password buffer lane 3
 *   Ba[496..567]: salt buffer lane 0 (72 uint = 288 bytes)
 *   Bb[568..639]: salt buffer lane 1
 *   Bc[640..711]: salt buffer lane 2
 *   Bd[712..783]: salt buffer lane 3
 */
void neoscrypt_fastkdf_4way(const uchar *password, const uchar *salt,
  uchar *output, uchar *scratchpad, const uint mode) {
    uint bufptr_a = 0, bufptr_b = 0, bufptr_c = 0, bufptr_d = 0;
    uint output_len, i, j;
    uint *T;
    uchar *Aa, *Ab, *Ac, *Ad;
    uchar *Ba, *Bb, *Bc, *Bd;

    T  = (uint *) &scratchpad[0];
    Aa = (uchar *) &T[176];
    Ab = (uchar *) &T[256];
    Ac = (uchar *) &T[336];
    Ad = (uchar *) &T[416];
    Ba = (uchar *) &T[496];
    Bb = (uchar *) &T[568];
    Bc = (uchar *) &T[640];
    Bd = (uchar *) &T[712];

    neoscrypt_copy(&Aa[0],   &password[0],   80);
    neoscrypt_copy(&Aa[80],  &password[0],   80);
    neoscrypt_copy(&Aa[160], &password[0],   80);
    neoscrypt_copy(&Aa[240], &password[0],   16);
    neoscrypt_copy(&Aa[256], &password[0],   64);
    neoscrypt_copy(&Ab[0],   &password[80],  80);
    neoscrypt_copy(&Ab[80],  &password[80],  80);
    neoscrypt_copy(&Ab[160], &password[80],  80);
    neoscrypt_copy(&Ab[240], &password[80],  16);
    neoscrypt_copy(&Ab[256], &password[80],  64);
    neoscrypt_copy(&Ac[0],   &password[160], 80);
    neoscrypt_copy(&Ac[80],  &password[160], 80);
    neoscrypt_copy(&Ac[160], &password[160], 80);
    neoscrypt_copy(&Ac[240], &password[160], 16);
    neoscrypt_copy(&Ac[256], &password[160], 64);
    neoscrypt_copy(&Ad[0],   &password[240], 80);
    neoscrypt_copy(&Ad[80],  &password[240], 80);
    neoscrypt_copy(&Ad[160], &password[240], 80);
    neoscrypt_copy(&Ad[240], &password[240], 16);
    neoscrypt_copy(&Ad[256], &password[240], 64);

    if(!mode) {
        output_len = 256;
        neoscrypt_copy(&Ba[0],   &salt[0],   80);
        neoscrypt_copy(&Ba[80],  &salt[0],   80);
        neoscrypt_copy(&Ba[160], &salt[0],   80);
        neoscrypt_copy(&Ba[240], &salt[0],   16);
        neoscrypt_copy(&Ba[256], &salt[0],   32);
        neoscrypt_copy(&Bb[0],   &salt[80],  80);
        neoscrypt_copy(&Bb[80],  &salt[80],  80);
        neoscrypt_copy(&Bb[160], &salt[80],  80);
        neoscrypt_copy(&Bb[240], &salt[80],  16);
        neoscrypt_copy(&Bb[256], &salt[80],  32);
        neoscrypt_copy(&Bc[0],   &salt[160], 80);
        neoscrypt_copy(&Bc[80],  &salt[160], 80);
        neoscrypt_copy(&Bc[160], &salt[160], 80);
        neoscrypt_copy(&Bc[240], &salt[160], 16);
        neoscrypt_copy(&Bc[256], &salt[160], 32);
        neoscrypt_copy(&Bd[0],   &salt[240], 80);
        neoscrypt_copy(&Bd[80],  &salt[240], 80);
        neoscrypt_copy(&Bd[160], &salt[240], 80);
        neoscrypt_copy(&Bd[240], &salt[240], 16);
        neoscrypt_copy(&Bd[256], &salt[240], 32);
    } else {
        output_len = 32;
        neoscrypt_copy(&Ba[0],   &salt[0],   256);
        neoscrypt_copy(&Ba[256], &salt[0],   32);
        neoscrypt_copy(&Bb[0],   &salt[256], 256);
        neoscrypt_copy(&Bb[256], &salt[256], 32);
        neoscrypt_copy(&Bc[0],   &salt[512], 256);
        neoscrypt_copy(&Bc[256], &salt[512], 32);
        neoscrypt_copy(&Bd[0],   &salt[768], 256);
        neoscrypt_copy(&Bd[256], &salt[768], 32);
    }

    for(i = 0; i < 32; i++) {

        neoscrypt_copy(&T[0],  blake2s_IV_P_XOR_4way, 128);
        neoscrypt_erase(&T[32], 64);

        for(j = 0; j < 32; j += 8) {
            T[j + 48] = *((const uint *) &Ba[bufptr_a + j]);
            T[j + 49] = *((const uint *) &Bb[bufptr_b + j]);
            T[j + 50] = *((const uint *) &Bc[bufptr_c + j]);
            T[j + 51] = *((const uint *) &Bd[bufptr_d + j]);
            T[j + 52] = *((const uint *) &Ba[bufptr_a + j + 4]);
            T[j + 53] = *((const uint *) &Bb[bufptr_b + j + 4]);
            T[j + 54] = *((const uint *) &Bc[bufptr_c + j + 4]);
            T[j + 55] = *((const uint *) &Bd[bufptr_d + j + 4]);
        }
        neoscrypt_erase(&T[80], 128);

        T[32] = 64; T[33] = 64; T[34] = 64; T[35] = 64;
        blake2s_compress_4way(&T[0]);

        for(j = 0; j < 64; j += 8) {
            T[j + 48] = *((const uint *) &Aa[bufptr_a + j]);
            T[j + 49] = *((const uint *) &Ab[bufptr_b + j]);
            T[j + 50] = *((const uint *) &Ac[bufptr_c + j]);
            T[j + 51] = *((const uint *) &Ad[bufptr_d + j]);
            T[j + 52] = *((const uint *) &Aa[bufptr_a + j + 4]);
            T[j + 53] = *((const uint *) &Ab[bufptr_b + j + 4]);
            T[j + 54] = *((const uint *) &Ac[bufptr_c + j + 4]);
            T[j + 55] = *((const uint *) &Ad[bufptr_d + j + 4]);
        }

        T[32] = 128; T[33] = 128; T[34] = 128; T[35] = 128;
        T[40] = ~0U; T[41] = ~0U; T[42] = ~0U; T[43] = ~0U;
        blake2s_compress_4way(&T[0]);

        bufptr_a = 0; bufptr_b = 0; bufptr_c = 0; bufptr_d = 0;
        for(j = 0; j < 32; j += 4) {
            bufptr_a += T[j];       bufptr_a += (T[j]     >> 8);
            bufptr_a += (T[j] >> 16); bufptr_a += (T[j]   >> 24);
            bufptr_b += T[j + 1];   bufptr_b += (T[j + 1] >> 8);
            bufptr_b += (T[j + 1] >> 16); bufptr_b += (T[j + 1] >> 24);
            bufptr_c += T[j + 2];   bufptr_c += (T[j + 2] >> 8);
            bufptr_c += (T[j + 2] >> 16); bufptr_c += (T[j + 2] >> 24);
            bufptr_d += T[j + 3];   bufptr_d += (T[j + 3] >> 8);
            bufptr_d += (T[j + 3] >> 16); bufptr_d += (T[j + 3] >> 24);
        }
        bufptr_a &= 0xFF; bufptr_b &= 0xFF;
        bufptr_c &= 0xFF; bufptr_d &= 0xFF;

        for(j = 0; j < 32; j += 8) {
            *((uint *) &Ba[bufptr_a + j])     ^= T[j];
            *((uint *) &Bb[bufptr_b + j])     ^= T[j + 1];
            *((uint *) &Bc[bufptr_c + j])     ^= T[j + 2];
            *((uint *) &Bd[bufptr_d + j])     ^= T[j + 3];
            *((uint *) &Ba[bufptr_a + j + 4]) ^= T[j + 4];
            *((uint *) &Bb[bufptr_b + j + 4]) ^= T[j + 5];
            *((uint *) &Bc[bufptr_c + j + 4]) ^= T[j + 6];
            *((uint *) &Bd[bufptr_d + j + 4]) ^= T[j + 7];
        }

        if(bufptr_a < 32)
            neoscrypt_copy(&Ba[256 + bufptr_a], &Ba[bufptr_a], 32 - bufptr_a);
        else if(bufptr_a > 224)
            neoscrypt_copy(&Ba[0], &Ba[256], bufptr_a - 224);
        if(bufptr_b < 32)
            neoscrypt_copy(&Bb[256 + bufptr_b], &Bb[bufptr_b], 32 - bufptr_b);
        else if(bufptr_b > 224)
            neoscrypt_copy(&Bb[0], &Bb[256], bufptr_b - 224);
        if(bufptr_c < 32)
            neoscrypt_copy(&Bc[256 + bufptr_c], &Bc[bufptr_c], 32 - bufptr_c);
        else if(bufptr_c > 224)
            neoscrypt_copy(&Bc[0], &Bc[256], bufptr_c - 224);
        if(bufptr_d < 32)
            neoscrypt_copy(&Bd[256 + bufptr_d], &Bd[bufptr_d], 32 - bufptr_d);
        else if(bufptr_d > 224)
            neoscrypt_copy(&Bd[0], &Bd[256], bufptr_d - 224);
    }

    i = 256 - bufptr_a;
    if(i >= output_len) {
        neoscrypt_xor(&Ba[bufptr_a], &Aa[0], output_len);
        neoscrypt_copy(&output[0], &Ba[bufptr_a], output_len);
    } else {
        neoscrypt_xor(&Ba[bufptr_a], &Aa[0], i);
        neoscrypt_xor(&Ba[0], &Aa[i], output_len - i);
        neoscrypt_copy(&output[0], &Ba[bufptr_a], i);
        neoscrypt_copy(&output[i], &Ba[0], output_len - i);
    }
    i = 256 - bufptr_b;
    if(i >= output_len) {
        neoscrypt_xor(&Bb[bufptr_b], &Ab[0], output_len);
        neoscrypt_copy(&output[output_len], &Bb[bufptr_b], output_len);
    } else {
        neoscrypt_xor(&Bb[bufptr_b], &Ab[0], i);
        neoscrypt_xor(&Bb[0], &Ab[i], output_len - i);
        neoscrypt_copy(&output[output_len], &Bb[bufptr_b], i);
        neoscrypt_copy(&output[output_len + i], &Bb[0], output_len - i);
    }
    i = 256 - bufptr_c;
    if(i >= output_len) {
        neoscrypt_xor(&Bc[bufptr_c], &Ac[0], output_len);
        neoscrypt_copy(&output[2 * output_len], &Bc[bufptr_c], output_len);
    } else {
        neoscrypt_xor(&Bc[bufptr_c], &Ac[0], i);
        neoscrypt_xor(&Bc[0], &Ac[i], output_len - i);
        neoscrypt_copy(&output[2 * output_len], &Bc[bufptr_c], i);
        neoscrypt_copy(&output[2 * output_len + i], &Bc[0], output_len - i);
    }
    i = 256 - bufptr_d;
    if(i >= output_len) {
        neoscrypt_xor(&Bd[bufptr_d], &Ad[0], output_len);
        neoscrypt_copy(&output[3 * output_len], &Bd[bufptr_d], output_len);
    } else {
        neoscrypt_xor(&Bd[bufptr_d], &Ad[0], i);
        neoscrypt_xor(&Bd[0], &Ad[i], output_len - i);
        neoscrypt_copy(&output[3 * output_len], &Bd[bufptr_d], i);
        neoscrypt_copy(&output[3 * output_len + i], &Bd[0], output_len - i);
    }
}

#endif /* defined(ASM) && defined(MINER_4WAY) */


/* ======================================================================== */
/* SIMD capability query                                                      */
/* ======================================================================== */

#ifndef ASM
/*
 * cpu_vec_exts - returns a bitmask of SIMD extensions available at runtime.
 *
 * In the portable C build (ASM not defined) no vector extensions are used.
 * The ASM build provides a platform-specific implementation that probes
 * CPUID (x86_64) or HWCAP (aarch64).
 */
uint cpu_vec_exts(void) {
    return 0;
}
#endif /* !ASM */
