#include "manifest_v2.h"
#include <stdlib.h>

#define GCD_COMPUTE(a, b) ({ \
  uint32_t _a = (a), _b = (b); \
  while (_b) { uint32_t _t = _b; _b = _a % _b; _a = _t; } \
  _a; \
})

#define PHI_SCALE_BITS 16

static inline void memzero_v2(void *p, size_t len) {
  uint8_t *b = (uint8_t *)p;
  for (size_t i = 0; i < len; i++) b[i] = 0;
}

static inline uint32_t crc32c_byte_branchless(uint32_t crc, uint8_t byte) {
  const uint32_t poly = 0x82F63B78U;
  crc ^= byte;
  for (int i = 0; i < 8; i++) {
    uint32_t mask = -(crc & 1);
    crc = (crc >> 1) ^ (poly & mask);
  }
  return crc;
}

static inline uint32_t crc32c_buffer_v2(const void *buf, size_t len, uint32_t crc) {
  const uint8_t *bytes = (const uint8_t *)buf;
  for (size_t i = 0; i < len; i++) {
    crc = crc32c_byte_branchless(crc, bytes[i]);
  }
  return crc;
}

int termux_manifest_v2_entry_validate_invariants(struct termux_manifest_entry_v2 *entry,
                                                   uint32_t total_entries) {
  if (!entry || total_entries == 0) return -1;

  if (entry->dep_count > TERMUX_MANIFEST_MAX_DEPS) return -5;
  if (entry->toroidal_depth >= 32) return -7;
  if (entry->coherence_phi > ((1ULL << 48) - 1)) return -3;

  for (uint16_t i = 0; i < entry->dep_count; i++) {
    if (entry->deps[i] >= total_entries) return -4;
  }

  uint32_t gcd_depth_32 = GCD_COMPUTE(entry->toroidal_depth, 32);
  uint8_t valid_gcds[] = {1, 2, 4, 8, 16, 32};
  int gcd_valid = 0;
  for (size_t i = 0; i < 6; i++) {
    if (gcd_depth_32 == valid_gcds[i]) {
      gcd_valid = 1;
      break;
    }
  }
  if (!gcd_valid) return -2;

  return 0;
}

int termux_manifest_v2_entry_compute_phi(struct termux_manifest_entry_v2 *entry,
                                          uint32_t toroidal_depth) {
  if (!entry || toroidal_depth >= 32) return -1;

  uint64_t depth_score = 32ULL - toroidal_depth;
  uint32_t gcd_val = GCD_COMPUTE(toroidal_depth + 1, 32);

  uint64_t coherence_base = (depth_score * (1ULL << PHI_SCALE_BITS)) / 32;
  uint64_t gcd_factor = (((uint64_t)gcd_val) * (1ULL << PHI_SCALE_BITS)) / 32;

  uint64_t phi = (coherence_base * gcd_factor) >> PHI_SCALE_BITS;

  entry->coherence_phi = phi > ((1ULL << 48) - 1) ? ((1ULL << 48) - 1) : phi;
  entry->toroidal_depth = toroidal_depth;

  return 0;
}

uint32_t termux_manifest_v2_entry_compute_crc32c(struct termux_manifest_entry_v2 *entry) {
  if (!entry) return 0;

  uint32_t crc = 0xFFFFFFFFU;

  crc = crc32c_buffer_v2(entry->name, TERMUX_MANIFEST_PKG_NAME_LEN, crc);
  crc = crc32c_buffer_v2(entry->version, TERMUX_MANIFEST_PKG_VERSION_LEN, crc);
  crc = crc32c_buffer_v2(&entry->arch_flags, sizeof(entry->arch_flags), crc);
  crc = crc32c_buffer_v2(&entry->api_level, sizeof(entry->api_level), crc);
  crc = crc32c_buffer_v2(&entry->build_flags, sizeof(entry->build_flags), crc);
  crc = crc32c_buffer_v2(entry->sha256, TERMUX_MANIFEST_SHA256_LEN, crc);
  crc = crc32c_buffer_v2(entry->deps, entry->dep_count * sizeof(uint16_t), crc);
  crc = crc32c_buffer_v2(&entry->toroidal_depth, sizeof(entry->toroidal_depth), crc);
  crc = crc32c_buffer_v2(&entry->dep_count, sizeof(entry->dep_count), crc);
  crc = crc32c_buffer_v2(&entry->coherence_phi, sizeof(entry->coherence_phi), crc);

  entry->crc32c = crc ^ 0xFFFFFFFFU;
  return entry->crc32c;
}

int termux_manifest_v2_validate_all(struct termux_manifest_entry_v2 *entries,
                                     uint32_t entry_count) {
  if (!entries || entry_count == 0 || entry_count > TERMUX_MANIFEST_MAX_ENTRIES) {
    return -1;
  }

  for (uint32_t i = 0; i < entry_count; i++) {
    int ret = termux_manifest_v2_entry_validate_invariants(&entries[i], entry_count);
    if (ret != 0) return ret;

    uint32_t crc_stored = entries[i].crc32c;
    uint32_t crc_computed = 0xFFFFFFFFU;
    crc_computed = crc32c_buffer_v2(entries[i].name, TERMUX_MANIFEST_PKG_NAME_LEN, crc_computed);
    crc_computed = crc32c_buffer_v2(entries[i].version, TERMUX_MANIFEST_PKG_VERSION_LEN, crc_computed);
    crc_computed = crc32c_buffer_v2(&entries[i].arch_flags, sizeof(entries[i].arch_flags), crc_computed);
    crc_computed = crc32c_buffer_v2(&entries[i].api_level, sizeof(entries[i].api_level), crc_computed);
    crc_computed = crc32c_buffer_v2(&entries[i].build_flags, sizeof(entries[i].build_flags), crc_computed);
    crc_computed = crc32c_buffer_v2(entries[i].sha256, TERMUX_MANIFEST_SHA256_LEN, crc_computed);
    crc_computed = crc32c_buffer_v2(entries[i].deps,
                                     entries[i].dep_count * sizeof(uint16_t),
                                     crc_computed);
    crc_computed = crc32c_buffer_v2(&entries[i].toroidal_depth,
                                     sizeof(entries[i].toroidal_depth),
                                     crc_computed);
    crc_computed = crc32c_buffer_v2(&entries[i].dep_count, sizeof(entries[i].dep_count), crc_computed);
    crc_computed = crc32c_buffer_v2(&entries[i].coherence_phi,
                                     sizeof(entries[i].coherence_phi),
                                     crc_computed);
    crc_computed ^= 0xFFFFFFFFU;

    if (crc_stored != crc_computed) return -6;
  }

  return 0;
}

uint32_t termux_manifest_v2_compute_global_crc32c(struct termux_manifest_entry_v2 *entries,
                                                   uint32_t entry_count) {
  if (!entries || entry_count == 0 || entry_count > TERMUX_MANIFEST_MAX_ENTRIES) {
    return 0;
  }

  uint32_t crc = 0xFFFFFFFFU;

  for (uint32_t i = 0; i < entry_count; i++) {
    crc = crc32c_buffer_v2(entries[i].name, TERMUX_MANIFEST_PKG_NAME_LEN, crc);
    crc = crc32c_buffer_v2(entries[i].version, TERMUX_MANIFEST_PKG_VERSION_LEN, crc);
    crc = crc32c_buffer_v2(&entries[i].arch_flags, sizeof(entries[i].arch_flags), crc);
    crc = crc32c_buffer_v2(&entries[i].api_level, sizeof(entries[i].api_level), crc);
    crc = crc32c_buffer_v2(&entries[i].build_flags, sizeof(entries[i].build_flags), crc);
    crc = crc32c_buffer_v2(entries[i].sha256, TERMUX_MANIFEST_SHA256_LEN, crc);
    crc = crc32c_buffer_v2(&entries[i].coherence_phi, sizeof(entries[i].coherence_phi), crc);
    crc = crc32c_buffer_v2(&entries[i].toroidal_depth, sizeof(entries[i].toroidal_depth), crc);
  }

  return crc ^ 0xFFFFFFFFU;
}

int termux_manifest_v2_load_from_buffer(const uint8_t *buf, size_t buflen,
                                         struct termux_manifest_entry_v2 *entries,
                                         uint32_t *entry_count) {
  if (!buf || !entries || !entry_count || buflen < sizeof(struct termux_manifest_v2)) {
    return -1;
  }

  struct termux_manifest_v2 *header = (struct termux_manifest_v2 *)buf;

  if (header->magic != TERMUX_MANIFEST_V2_MAGIC) return -2;
  if (header->version != TERMUX_MANIFEST_V2_VERSION) return -3;
  if (header->entry_count == 0 || header->entry_count > TERMUX_MANIFEST_MAX_ENTRIES) {
    return -4;
  }

  size_t expected_size = sizeof(struct termux_manifest_v2) +
                        (header->entry_count * sizeof(struct termux_manifest_entry_v2));
  if (buflen < expected_size) return -5;

  struct termux_manifest_entry_v2 *src_entries =
      (struct termux_manifest_entry_v2 *)(buf + sizeof(struct termux_manifest_v2));

  for (uint32_t i = 0; i < header->entry_count; i++) {
    memzero_v2(&entries[i], sizeof(entries[i]));

    const struct termux_manifest_entry_v2 *src = &src_entries[i];
    struct termux_manifest_entry_v2 *dst = &entries[i];

    for (size_t j = 0; j < TERMUX_MANIFEST_PKG_NAME_LEN; j++) {
      dst->name[j] = src->name[j];
    }
    for (size_t j = 0; j < TERMUX_MANIFEST_PKG_VERSION_LEN; j++) {
      dst->version[j] = src->version[j];
    }

    dst->arch_flags = src->arch_flags;
    dst->api_level = src->api_level;
    dst->build_flags = src->build_flags;

    for (size_t j = 0; j < TERMUX_MANIFEST_SHA256_LEN; j++) {
      dst->sha256[j] = src->sha256[j];
    }

    dst->crc32c = src->crc32c;
    dst->coherence_phi = src->coherence_phi;
    dst->toroidal_depth = src->toroidal_depth;
    dst->dep_count = src->dep_count;

    for (uint16_t j = 0; j < dst->dep_count && j < TERMUX_MANIFEST_MAX_DEPS; j++) {
      dst->deps[j] = src->deps[j];
    }

    dst->phase_mask = src->phase_mask;
  }

  int ret = termux_manifest_v2_validate_all(entries, header->entry_count);
  if (ret != 0) return ret;

  *entry_count = header->entry_count;
  return 0;
}

/* DFS-based cycle detection */
static int termux_manifest_dfs_visit(uint32_t node,
                                      struct termux_manifest_entry_v2 *entries,
                                      uint32_t entry_count,
                                      uint8_t *state) {
  state[node] = 1;  /* Mark as visiting */

  struct termux_manifest_entry_v2 *entry = &entries[node];
  for (uint16_t i = 0; i < entry->dep_count; i++) {
    uint16_t dep = entry->deps[i];

    if (dep >= entry_count) continue;  /* Skip invalid (should be caught by validation) */

    if (state[dep] == 1) {  /* Back edge = cycle */
      return -3;
    }

    if (state[dep] == 0) {  /* Unvisited */
      int ret = termux_manifest_dfs_visit(dep, entries, entry_count, state);
      if (ret != 0) return ret;
    }
  }

  state[node] = 2;  /* Mark as visited */
  return 0;
}

/*
 * termux_resolve_manifest_dependencies — Validate dependency graph
 *
 * Returns:
 *   0 on success
 *  -1 if entries is NULL
 *  -2 if dependency index out of bounds
 */
int termux_resolve_manifest_dependencies(struct termux_manifest_entry_v2 *entries,
                                         uint32_t entry_count) {
  if (!entries && entry_count > 0) return -1;
  if (entry_count == 0) return 0;

  /* Validate all dependencies are within bounds */
  for (uint32_t i = 0; i < entry_count; i++) {
    struct termux_manifest_entry_v2 *entry = &entries[i];
    if (entry->dep_count > TERMUX_MANIFEST_MAX_DEPS) return -2;

    for (uint16_t j = 0; j < entry->dep_count; j++) {
      if (entry->deps[j] >= entry_count) return -2;
    }
  }

  return 0;
}

/*
 * termux_detect_manifest_circular_deps — Detect circular dependencies
 *
 * Returns:
 *   0 if no cycles
 *  -1 if entries is NULL
 *  -3 if circular dependency detected
 */
int termux_detect_manifest_circular_deps(struct termux_manifest_entry_v2 *entries,
                                         uint32_t entry_count) {
  if (!entries && entry_count > 0) return -1;
  if (entry_count == 0) return 0;

  uint8_t *state = (uint8_t *)calloc(entry_count, sizeof(uint8_t));
  if (!state) return -1;

  int result = 0;

  for (uint32_t i = 0; i < entry_count && result == 0; i++) {
    if (state[i] == 0) {
      result = termux_manifest_dfs_visit(i, entries, entry_count, state);
    }
  }

  free(state);
  return result;
}
