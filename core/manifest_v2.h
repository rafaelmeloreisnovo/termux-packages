#ifndef TERMUX_MANIFEST_V2_H
#define TERMUX_MANIFEST_V2_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#define TERMUX_MANIFEST_V2_MAGIC 0x4D46564DU
#define TERMUX_MANIFEST_V2_VERSION 2

#define TERMUX_MANIFEST_PKG_NAME_LEN 64
#define TERMUX_MANIFEST_PKG_VERSION_LEN 32
#define TERMUX_MANIFEST_MAX_DEPS 16
#define TERMUX_MANIFEST_MAX_ENTRIES 2057
#define TERMUX_MANIFEST_SHA256_LEN 32
#define TERMUX_MANIFEST_ENTRY_SIZE 200

#pragma pack(push, 1)

struct termux_manifest_entry_v2 {
  char name[TERMUX_MANIFEST_PKG_NAME_LEN];          // @ 0 (64 bytes)
  char version[TERMUX_MANIFEST_PKG_VERSION_LEN];    // @ 64 (32 bytes)
  uint32_t arch_flags;                              // @ 96 (4 bytes)
  uint32_t api_level;                               // @ 100 (4 bytes)
  uint32_t build_flags;                             // @ 104 (4 bytes)
  uint8_t sha256[TERMUX_MANIFEST_SHA256_LEN];       // @ 108 (32 bytes)
  uint32_t crc32c;                                  // @ 140 (4 bytes)
  uint64_t coherence_phi;                           // @ 144 (8 bytes)
  uint16_t toroidal_depth;                          // @ 152 (2 bytes)
  uint16_t dep_count;                               // @ 154 (2 bytes)
  uint16_t deps[TERMUX_MANIFEST_MAX_DEPS];          // @ 156 (32 bytes)
  uint64_t phase_mask;                              // @ 188 (8 bytes)
  uint32_t _reserved;                               // @ 196 (4 bytes) padding
};

_Static_assert(sizeof(struct termux_manifest_entry_v2) == TERMUX_MANIFEST_ENTRY_SIZE,
               "termux_manifest_entry_v2 must be exactly 200 bytes");
_Static_assert(offsetof(struct termux_manifest_entry_v2, coherence_phi) == 144,
               "coherence_phi offset must be 144 bytes");
_Static_assert(offsetof(struct termux_manifest_entry_v2, phase_mask) == 188,
               "phase_mask offset must be 188 bytes");

struct termux_manifest_v2 {
  uint32_t magic;
  uint32_t version;
  uint32_t entry_count;
  uint32_t crc32c_checksum;
  uint64_t timestamp;
  uint8_t _reserved[32];
};

#pragma pack(pop)

typedef int (*termux_manifest_validator_fn)(struct termux_manifest_entry_v2 *);

int termux_manifest_v2_entry_validate_invariants(struct termux_manifest_entry_v2 *entry,
                                                   uint32_t total_entries);
int termux_manifest_v2_entry_compute_phi(struct termux_manifest_entry_v2 *entry,
                                          uint32_t toroidal_depth);
uint32_t termux_manifest_v2_entry_compute_crc32c(struct termux_manifest_entry_v2 *entry);

int termux_manifest_v2_validate_all(struct termux_manifest_entry_v2 *entries,
                                     uint32_t entry_count);
uint32_t termux_manifest_v2_compute_global_crc32c(struct termux_manifest_entry_v2 *entries,
                                                   uint32_t entry_count);

int termux_manifest_v2_load_from_buffer(const uint8_t *buf, size_t buflen,
                                         struct termux_manifest_entry_v2 *entries,
                                         uint32_t *entry_count);

int termux_resolve_manifest_dependencies(struct termux_manifest_entry_v2 *entries,
                                         uint32_t entry_count);
int termux_detect_manifest_circular_deps(struct termux_manifest_entry_v2 *entries,
                                         uint32_t entry_count);

#endif // TERMUX_MANIFEST_V2_H
