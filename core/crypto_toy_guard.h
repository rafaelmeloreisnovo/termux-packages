#ifndef RAF_CRYPTO_TOY_GUARD_H
#define RAF_CRYPTO_TOY_GUARD_H

/*
 * SECURITY CLASSIFICATION: SIMULATED_TOY
 *
 * These legacy crypto_* APIs are retained only for regression tests and
 * migration work. They do NOT implement Ed25519, ChaCha20-Poly1305,
 * HKDF-SHA256, ML-KEM/Kyber, ML-DSA/Dilithium, or any production-grade
 * cryptographic primitive.
 *
 * Invariants:
 *   - security claim is always false;
 *   - production use is forbidden;
 *   - a build that declares RAF_PRODUCTION_BUILD must fail at compile time.
 */

#define RAF_CRYPTO_IMPLEMENTATION_CLASS_SIMULATED_TOY 1
#define RAF_CRYPTO_SECURITY_CLAIM_ALLOWED 0
#define RAF_CRYPTO_PRODUCTION_ALLOWED 0

#ifdef RAF_PRODUCTION_BUILD
#error "SIMULATED_TOY crypto is forbidden in RAF_PRODUCTION_BUILD; replace with a vetted implementation"
#endif

#endif /* RAF_CRYPTO_TOY_GUARD_H */
