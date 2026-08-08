#ifndef CRYPTO_CHACHA20_H
#define CRYPTO_CHACHA20_H

/*
 * LEGACY TEST API — SIMULATED_TOY, NOT ChaCha20-Poly1305/HKDF-SHA256.
 *
 * Symbol names are preserved temporarily for historical test compatibility.
 * The implementation uses XOR/simple tag logic and rand(); it is not AEAD,
 * does not provide secure key/nonce generation, and must not be used to protect
 * data. Production inclusion is fail-closed by crypto_toy_guard.h.
 */

#include "crypto_toy_guard.h"
#include <stddef.h>
#include <stdint.h>

#define RAF_CHACHA20_API_IS_TOY 1

typedef struct {
  uint8_t key[32];
  uint8_t nonce[12];
} chacha20_cipher_t;

typedef struct {
  uint8_t ciphertext[4096];
  size_t ciphertext_len;
  uint8_t tag[16];
  uint64_t timestamp_ms;
} chacha20_sealed_t;

typedef struct {
  uint8_t plaintext[4096];
  size_t plaintext_len;
} chacha20_opened_t;

/* Toy initialization only. */
int chacha20_init(chacha20_cipher_t *cipher, const uint8_t *key,
                  const uint8_t *nonce);

/* Toy reversible transform + weak tag. NOT authenticated encryption. */
int chacha20_seal(const chacha20_cipher_t *cipher, const uint8_t *plaintext,
                  size_t plaintext_len, const uint8_t *aad, size_t aad_len,
                  chacha20_sealed_t *sealed);

/* Toy consistency check + reverse transform. NOT AEAD verification. */
int chacha20_open(const chacha20_cipher_t *cipher,
                  const chacha20_sealed_t *sealed, const uint8_t *aad,
                  size_t aad_len, chacha20_opened_t *opened);

/* Uses rand(); NOT a cryptographically secure key generator. */
int chacha20_keygen(uint8_t *key);

/* Uses rand(); NOT a cryptographically secure nonce generator. */
int chacha20_noncegen(uint8_t *nonce);

/* Toy derivation only. NOT HKDF-SHA256. */
int chacha20_derive_key(const char *passphrase, size_t passphrase_len,
                        const uint8_t *salt, size_t salt_len, uint8_t *key);

typedef struct {
  uint32_t seal_count;
  uint32_t open_count;
  uint32_t open_failures;
  uint64_t total_bytes_encrypted;
  uint64_t total_bytes_decrypted;
  double mean_encryption_time_us;
  double mean_decryption_time_us;
} chacha20_stats_t;

int chacha20_get_stats(chacha20_stats_t *stats);

#endif /* CRYPTO_CHACHA20_H */
