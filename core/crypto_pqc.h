#ifndef CRYPTO_PQC_H
#define CRYPTO_PQC_H

/*
 * Phase 9.21: Post-Quantum Cryptography
 * Future-proof key exchange and signatures
 * Supports hybrid classical + PQC schemes
 */

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Post-Quantum Key Exchange (KYBER-like)
 * ============================================================================ */

#define KYBER_PUBLICKEY_BYTES 1184
#define KYBER_CIPHERTEXT_BYTES 1088
#define KYBER_SHAREDSECRET_BYTES 32

typedef struct {
  uint8_t public_key[KYBER_PUBLICKEY_BYTES];
  uint8_t secret_key[3168];  /* Full secret key for decapsulation */
} pqc_kem_keypair_t;

typedef struct {
  uint8_t ciphertext[KYBER_CIPHERTEXT_BYTES];
  uint8_t shared_secret[KYBER_SHAREDSECRET_BYTES];
} pqc_kem_encapsulation_t;

/* ============================================================================
 * Post-Quantum Signatures (DILITHIUM-like)
 * ============================================================================ */

#define DILITHIUM_PUBLICKEY_BYTES 1312
#define DILITHIUM_SIGNATURE_BYTES 2420

typedef struct {
  uint8_t public_key[DILITHIUM_PUBLICKEY_BYTES];
  uint8_t secret_key[2544];  /* Full secret key */
} pqc_sig_keypair_t;

typedef struct {
  uint8_t signature[DILITHIUM_SIGNATURE_BYTES];
} pqc_sig_t;

/* ============================================================================
 * Hybrid Scheme (Classical + PQC)
 * ============================================================================ */

typedef struct {
  uint8_t ed25519_public[32];           /* Classical: Ed25519 */
  uint8_t pqc_sig_public[DILITHIUM_PUBLICKEY_BYTES]; /* PQC signature */
  uint8_t kyber_public[KYBER_PUBLICKEY_BYTES];       /* PQC KEM */
} pqc_hybrid_public_key_t;

typedef struct {
  uint8_t ed25519_private[32];
  uint8_t pqc_sig_private[2544];
  uint8_t kyber_private[3168];
} pqc_hybrid_private_key_t;

/* ============================================================================
 * PQC Key Encapsulation
 * ============================================================================ */

/* Generate PQC KEM keypair */
int pqc_kem_keygen(pqc_kem_keypair_t *keypair);

/* Encapsulate shared secret (public key encryption) */
int pqc_kem_encapsulate(const uint8_t *public_key,
                        pqc_kem_encapsulation_t *encap);

/* Decapsulate shared secret (private key decryption) */
int pqc_kem_decapsulate(const uint8_t *secret_key,
                        const uint8_t *ciphertext,
                        uint8_t *shared_secret);

/* ============================================================================
 * PQC Signatures
 * ============================================================================ */

/* Generate PQC signature keypair */
int pqc_sig_keygen(pqc_sig_keypair_t *keypair);

/* Sign message with PQC private key */
int pqc_sig_sign(const uint8_t *secret_key, const uint8_t *message,
                 size_t message_len, pqc_sig_t *signature);

/* Verify signature with PQC public key */
int pqc_sig_verify(const uint8_t *public_key, const uint8_t *message,
                   size_t message_len, const pqc_sig_t *signature);

/* ============================================================================
 * Hybrid Scheme Operations
 * ============================================================================ */

/* Generate hybrid keypair (Ed25519 + PQC) */
int pqc_hybrid_keygen(pqc_hybrid_private_key_t *private_key,
                      pqc_hybrid_public_key_t *public_key);

/* Hybrid sign (sign with both Ed25519 and DILITHIUM) */
int pqc_hybrid_sign(const pqc_hybrid_private_key_t *private_key,
                    const uint8_t *message, size_t message_len,
                    uint8_t *hybrid_signature);

/* Hybrid verify (verify both signatures) */
int pqc_hybrid_verify(const pqc_hybrid_public_key_t *public_key,
                      const uint8_t *message, size_t message_len,
                      const uint8_t *hybrid_signature);

/* ============================================================================
 * Migration & Compatibility
 * ============================================================================ */

typedef enum {
  PQC_SCHEME_CLASSICAL,    /* Ed25519 only */
  PQC_SCHEME_HYBRID,       /* Ed25519 + DILITHIUM */
  PQC_SCHEME_PQC_ONLY      /* DILITHIUM only */
} pqc_scheme_t;

/* Get current PQC scheme in use */
pqc_scheme_t pqc_get_scheme(void);

/* Set PQC scheme (migration support) */
int pqc_set_scheme(pqc_scheme_t scheme);

/* ============================================================================
 * Statistics & Monitoring
 * ============================================================================ */

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

/* Get PQC statistics */
int pqc_get_stats(pqc_stats_t *stats);

#endif /* CRYPTO_PQC_H */
