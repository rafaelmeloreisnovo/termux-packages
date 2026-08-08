#ifndef CRYPTO_PQC_H
#define CRYPTO_PQC_H

/*
 * LEGACY TEST API — SIMULATED_TOY, NOT ML-KEM/Kyber or ML-DSA/Dilithium.
 *
 * Sizes and legacy symbol names mimic historical experiments only. The
 * implementation uses rand()/XOR-style behavior and provides no post-quantum
 * security guarantee. Production inclusion is fail-closed by
 * crypto_toy_guard.h.
 */

#include "crypto_toy_guard.h"
#include <stddef.h>
#include <stdint.h>

#define RAF_PQC_API_IS_TOY 1

#define KYBER_PUBLICKEY_BYTES 1184
#define KYBER_CIPHERTEXT_BYTES 1088
#define KYBER_SHAREDSECRET_BYTES 32

typedef struct {
  uint8_t public_key[KYBER_PUBLICKEY_BYTES];
  uint8_t secret_key[3168];
} pqc_kem_keypair_t;

typedef struct {
  uint8_t ciphertext[KYBER_CIPHERTEXT_BYTES];
  uint8_t shared_secret[KYBER_SHAREDSECRET_BYTES];
} pqc_kem_encapsulation_t;

#define DILITHIUM_PUBLICKEY_BYTES 1312
#define DILITHIUM_SIGNATURE_BYTES 2420

typedef struct {
  uint8_t public_key[DILITHIUM_PUBLICKEY_BYTES];
  uint8_t secret_key[2544];
} pqc_sig_keypair_t;

typedef struct {
  uint8_t signature[DILITHIUM_SIGNATURE_BYTES];
} pqc_sig_t;

typedef struct {
  uint8_t ed25519_public[32];
  uint8_t pqc_sig_public[DILITHIUM_PUBLICKEY_BYTES];
  uint8_t kyber_public[KYBER_PUBLICKEY_BYTES];
} pqc_hybrid_public_key_t;

typedef struct {
  uint8_t ed25519_private[32];
  uint8_t pqc_sig_private[2544];
  uint8_t kyber_private[3168];
} pqc_hybrid_private_key_t;

/* All operations below are toy compatibility operations, not PQ primitives. */
int pqc_kem_keygen(pqc_kem_keypair_t *keypair);
int pqc_kem_encapsulate(const uint8_t *public_key,
                        pqc_kem_encapsulation_t *encap);
int pqc_kem_decapsulate(const uint8_t *secret_key,
                        const uint8_t *ciphertext,
                        uint8_t *shared_secret);

int pqc_sig_keygen(pqc_sig_keypair_t *keypair);
int pqc_sig_sign(const uint8_t *secret_key, const uint8_t *message,
                 size_t message_len, pqc_sig_t *signature);
int pqc_sig_verify(const uint8_t *public_key, const uint8_t *message,
                   size_t message_len, const pqc_sig_t *signature);

int pqc_hybrid_keygen(pqc_hybrid_private_key_t *private_key,
                      pqc_hybrid_public_key_t *public_key);
int pqc_hybrid_sign(const pqc_hybrid_private_key_t *private_key,
                    const uint8_t *message, size_t message_len,
                    uint8_t *hybrid_signature);
int pqc_hybrid_verify(const pqc_hybrid_public_key_t *public_key,
                      const uint8_t *message, size_t message_len,
                      const uint8_t *hybrid_signature);

typedef enum {
  PQC_SCHEME_CLASSICAL,
  PQC_SCHEME_HYBRID,
  PQC_SCHEME_PQC_ONLY
} pqc_scheme_t;

pqc_scheme_t pqc_get_scheme(void);
int pqc_set_scheme(pqc_scheme_t scheme);

typedef struct {
  uint32_t kem_encap_count;
  uint32_t kem_decap_count;
  uint32_t sig_sign_count;
  uint32_t sig_verify_count;
  uint32_t hybrid_sign_count;
  uint32_t hybrid_verify_count;
  uint32_t verify_failures;
  pqc_scheme_t current_scheme;
} pqc_stats_t;

int pqc_get_stats(pqc_stats_t *stats);

#endif /* CRYPTO_PQC_H */
