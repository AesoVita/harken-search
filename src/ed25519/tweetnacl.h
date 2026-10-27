// TweetNaCl — Ed25519 signature verification subset
// Original: Daniel J. Bernstein et al. — Public domain.
// Stripped to verify-only (no keypair generation, no signing).
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define crypto_sign_BYTES 64
#define crypto_sign_PUBLICKEYBYTES 32

// Verify a signed message.
// Returns 0 on success, -1 on failure.
// m:    output buffer for the verified message (at least smlen bytes)
// mlen: output — length of the verified message
// sm:   signed message (signature || message), length smlen
// pk:   public key (32 bytes)
int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                     const unsigned char *sm, unsigned long long smlen,
                     const unsigned char *pk);

// SHA-512 hash (needed internally by Ed25519 verify)
int crypto_hash(unsigned char *out, const unsigned char *m, unsigned long long n);

#ifdef __cplusplus
}
#endif
