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

#ifndef BITCOIN_CRYPTO_NEOSCRYPT_H
#define BITCOIN_CRYPTO_NEOSCRYPT_H

/*
 * Pull in the C standard integer and size types.  These are required by
 * C99 (ISO/IEC 9899:1999) §7.18 and §7.17 respectively.  All compilers
 * targeting GoByte-supported platforms (Linux/macOS/Windows, x86_64/aarch64)
 * provide them.
 */
#include <stddef.h>  /* size_t */
#include <stdint.h>  /* uintptr_t, uint32_t, uint64_t */

/*
 * Portable type aliases used throughout the NeoScrypt implementation.
 *
 * NOTE: POSIX (<sys/types.h>) may also define `uint` when _BSD_SOURCE or
 * _DEFAULT_SOURCE is active.  If a redefinition diagnostic fires, suppress
 * it by ensuring this header is included before <sys/types.h>, or by
 * replacing these aliases with their explicit <stdint.h> equivalents
 * (uint32_t, uint8_t, etc.) in a future cleanup pass.
 */
typedef unsigned long long  ullong;
typedef signed long long    llong;
typedef unsigned int        uint;
typedef unsigned char       uchar;

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* NeoScrypt / BLAKE2s internal block and digest sizes, in bytes. */
#define BLOCK_SIZE   64U
#define DIGEST_SIZE  32U

typedef uchar hash_digest[DIGEST_SIZE];

/*
 * 32-bit rotate macros.
 *
 * The explicit (uint) cast on `a` ensures the shift is performed on an
 * unsigned 32-bit value regardless of the caller's expression type,
 * preventing undefined behaviour on signed integers and suppressing
 * implicit-conversion warnings on platforms where int > 32 bits.
 *
 * Rotation counts are unsigned constants in [1, 31] at every call site;
 * no range guard is needed here.
 */
#define ROTL32(a, b) (((uint)(a) << (b)) | ((uint)(a) >> (32 - (b))))
#define ROTR32(a, b) (((uint)(a) >> (b)) | ((uint)(a) << (32 - (b))))

/*
 * Big-endian serialisation helpers.
 *
 * U8TO32_BE: read four bytes from pointer `p` as a big-endian uint32.
 * U32TO8_BE: write uint32 `v` as four big-endian bytes at pointer `p`.
 * U64TO8_BE: write uint64 `v` as eight big-endian bytes at pointer `p`.
 *
 * These are implemented as portable byte-shift operations and are correct
 * on both little-endian (x86_64) and big-endian architectures.  They do
 * NOT assume a particular machine byte order.
 */
#define U8TO32_BE(p) \
    (((uint)((p)[0]) << 24) | ((uint)((p)[1]) << 16) | \
     ((uint)((p)[2]) <<  8) | ((uint)((p)[3])       ))

#define U32TO8_BE(p, v) \
    (p)[0] = (uchar)((v) >> 24); \
    (p)[1] = (uchar)((v) >> 16); \
    (p)[2] = (uchar)((v) >>  8); \
    (p)[3] = (uchar)((v)       );

#define U64TO8_BE(p, v) \
    U32TO8_BE((p),     (uint)((v) >> 32)); \
    U32TO8_BE((p) + 4, (uint)((v)       ));

/*
 * Public API — wrapped in extern "C" so the header is usable from both
 * C and C++ translation units without name mangling.
 */
#if defined(__cplusplus)
extern "C" {
#endif

/*
 * neoscrypt - NeoScrypt proof-of-work hash function.
 *
 * password    : 80-byte block header (used as both password and salt).
 * output      : 32-byte output buffer.  On any internal failure (OOM)
 *               the buffer is zeroed before returning.
 * profile     : algorithm selector — GoByte always passes 0x0
 *               (NeoScrypt with FastKDF-BLAKE2s, Salsa20/20, ChaCha20/20).
 */
void neoscrypt(const unsigned char *password, unsigned char *output,
               unsigned int profile);

/*
 * neoscrypt_blake2s - single-call BLAKE2s with key.
 *
 * Produces `output_size` bytes of keyed hash into `output`.
 * key_size and output_size must both be <= BLOCK_SIZE (64).
 * Keys longer than BLOCK_SIZE are silently truncated.
 */
void neoscrypt_blake2s(const void *input,       const unsigned int  input_size,
                       const void *key,         const unsigned char key_size,
                       void       *output,      const unsigned char output_size);

/*
 * Low-level memory primitives.
 *
 * neoscrypt_copy  : equivalent to memcpy(dstp, srcp, len).
 * neoscrypt_erase : secure zero (volatile writes; not optimised away).
 * neoscrypt_xor   : dst[i] ^= src[i] for len bytes.
 *
 * When compiled with ASM=1 these are provided by the assembly backend;
 * the ASM implementation of neoscrypt_erase MUST use volatile stores.
 */
void neoscrypt_copy(void *dstp, const void *srcp, unsigned int len);
void neoscrypt_erase(void *dstp, unsigned int len);
void neoscrypt_xor(void *dstp, const void *srcp, unsigned int len);

/* cpu_vec_exts - returns a bitmask of SIMD extensions available at runtime.
 * Returns 0 when compiled without ASM support. */
unsigned int cpu_vec_exts(void);

/*
 * 4-way parallel miner API (requires ASM=1 and MINER_4WAY=1).
 * These are NOT part of the consensus-critical node build.
 */
#if defined(ASM) && defined(MINER_4WAY)

void neoscrypt_4way(const unsigned char *password, unsigned char *output,
                    unsigned char *scratchpad);

#ifdef SHA256
void scrypt_4way(const unsigned char *password, unsigned char *output,
                 unsigned char *scratchpad);
#endif

void neoscrypt_blake2s_4way(const unsigned char *input,
                             const unsigned char *key,
                             unsigned char       *output);

void neoscrypt_fastkdf_4way(const unsigned char *password,
                             const unsigned char *salt,
                             unsigned char       *output,
                             unsigned char       *scratchpad,
                             const unsigned int   mode);

#endif /* defined(ASM) && defined(MINER_4WAY) */

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* BITCOIN_CRYPTO_NEOSCRYPT_H */
