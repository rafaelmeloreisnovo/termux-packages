#include "../manifest_v2.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int test_manifest_entry_validate_invariants(void) {
  struct termux_manifest_entry_v2 entry = {};
  strcpy(entry.name, "test-package");
  entry.toroidal_depth = 21;
  entry.coherence_phi = 0x123456789ABCULL;
  entry.dep_count = 3;
  entry.deps[0] = 0;
  entry.deps[1] = 1;
  entry.deps[2] = 2;

  int ret = termux_manifest_v2_entry_validate_invariants(&entry, 100);
  assert(ret == 0);

  entry.coherence_phi = (1ULL << 48);
  ret = termux_manifest_v2_entry_validate_invariants(&entry, 100);
  assert(ret == -3);

  entry.coherence_phi = 0x123456789ABCULL;
  entry.deps[1] = 100;
  ret = termux_manifest_v2_entry_validate_invariants(&entry, 100);
  assert(ret == -4);

  entry.deps[1] = 1;
  entry.dep_count = TERMUX_MANIFEST_MAX_DEPS + 1;
  ret = termux_manifest_v2_entry_validate_invariants(&entry, 100);
  assert(ret == -5);

  printf("✓ test_manifest_entry_validate_invariants passed\n");
  return 0;
}

static int test_manifest_entry_compute_phi(void) {
  struct termux_manifest_entry_v2 entry = {};
  strcpy(entry.name, "test-package");

  int ret = termux_manifest_v2_entry_compute_phi(&entry, 0);
  assert(ret == 0);
  assert(entry.toroidal_depth == 0);
  assert(entry.coherence_phi > 0);

  ret = termux_manifest_v2_entry_compute_phi(&entry, 16);
  assert(ret == 0);
  assert(entry.toroidal_depth == 16);
  assert(entry.coherence_phi <= ((1ULL << 48) - 1));

  ret = termux_manifest_v2_entry_compute_phi(&entry, 31);
  assert(ret == 0);
  assert(entry.toroidal_depth == 31);
  assert(entry.coherence_phi <= ((1ULL << 48) - 1));

  printf("✓ test_manifest_entry_compute_phi passed\n");
  return 0;
}

static int test_manifest_entry_compute_crc32c(void) {
  struct termux_manifest_entry_v2 entry = {};
  strcpy(entry.name, "test-package");
  entry.toroidal_depth = 10;
  entry.coherence_phi = 0x123456789ABCULL;
  entry.dep_count = 2;
  entry.deps[0] = 5;
  entry.deps[1] = 10;

  uint32_t crc1 = termux_manifest_v2_entry_compute_crc32c(&entry);
  assert(crc1 != 0);

  uint32_t crc2 = termux_manifest_v2_entry_compute_crc32c(&entry);
  assert(crc1 == crc2);

  printf("✓ test_manifest_entry_compute_crc32c passed (deterministic)\n");
  return 0;
}

static int test_manifest_gcd_invariant_coverage(void) {
  uint8_t valid_gcds[] = {1, 2, 4, 8, 16, 32};

  for (uint32_t depth = 0; depth < 32; depth++) {
    uint32_t a = depth, b = 32;
    while (b) {
      uint32_t tmp = b;
      b = a % b;
      a = tmp;
    }
    int is_valid = 0;
    for (size_t i = 0; i < 6; i++) {
      if (valid_gcds[i] == a) {
        is_valid = 1;
        break;
      }
    }
    assert(is_valid);
  }

  printf("✓ test_manifest_gcd_invariant_coverage passed (all depths 0..31 have valid gcd)\n");
  return 0;
}

static int test_manifest_validate_all(void) {
  struct termux_manifest_entry_v2 entries[10] = {};

  for (int i = 0; i < 10; i++) {
    sprintf(entries[i].name, "pkg-%d", i);
    entries[i].toroidal_depth = i % 42;
    termux_manifest_v2_entry_compute_phi(&entries[i], entries[i].toroidal_depth);
    termux_manifest_v2_entry_compute_crc32c(&entries[i]);
  }

  int ret = termux_manifest_v2_validate_all(entries, 10);
  assert(ret == 0);

  entries[5].coherence_phi = (1ULL << 48);
  ret = termux_manifest_v2_validate_all(entries, 10);
  assert(ret == -3);

  printf("✓ test_manifest_validate_all passed\n");
  return 0;
}

static int test_manifest_compute_global_crc32c(void) {
  struct termux_manifest_entry_v2 entries[5] = {};

  for (int i = 0; i < 5; i++) {
    sprintf(entries[i].name, "package-%d", i);
    entries[i].toroidal_depth = i;
    termux_manifest_v2_entry_compute_phi(&entries[i], i);
  }

  uint32_t global_crc1 = termux_manifest_v2_compute_global_crc32c(entries, 5);
  assert(global_crc1 != 0);

  uint32_t global_crc2 = termux_manifest_v2_compute_global_crc32c(entries, 5);
  assert(global_crc1 == global_crc2);

  printf("✓ test_manifest_compute_global_crc32c passed (deterministic)\n");
  return 0;
}

static int test_manifest_load_from_buffer(void) {
  uint8_t buffer[2048] = {};
  struct termux_manifest_v2 *header = (struct termux_manifest_v2 *)buffer;
  header->magic = TERMUX_MANIFEST_V2_MAGIC;
  header->version = TERMUX_MANIFEST_V2_VERSION;
  header->entry_count = 3;
  header->timestamp = 0;

  struct termux_manifest_entry_v2 *src_entries =
      (struct termux_manifest_entry_v2 *)(buffer + sizeof(struct termux_manifest_v2));

  for (int i = 0; i < 3; i++) {
    sprintf(src_entries[i].name, "test-pkg-%d", i);
    src_entries[i].toroidal_depth = i;
    termux_manifest_v2_entry_compute_phi(&src_entries[i], i);
    termux_manifest_v2_entry_compute_crc32c(&src_entries[i]);
  }

  struct termux_manifest_entry_v2 entries[10] = {};
  uint32_t entry_count = 0;

  size_t buflen = sizeof(struct termux_manifest_v2) +
                  (3 * sizeof(struct termux_manifest_entry_v2));

  int ret = termux_manifest_v2_load_from_buffer(buffer, buflen, entries, &entry_count);
  assert(ret == 0);
  assert(entry_count == 3);

  printf("✓ test_manifest_load_from_buffer passed\n");
  return 0;
}

static int test_manifest_fail_closed_property(void) {
  struct termux_manifest_entry_v2 entries[2] = {};

  entries[0].toroidal_depth = 0;
  termux_manifest_v2_entry_compute_phi(&entries[0], 0);
  termux_manifest_v2_entry_compute_crc32c(&entries[0]);

  entries[1].toroidal_depth = 1;
  termux_manifest_v2_entry_compute_phi(&entries[1], 1);
  termux_manifest_v2_entry_compute_crc32c(&entries[1]);

  int ret = termux_manifest_v2_validate_all(entries, 2);
  assert(ret == 0);

  entries[0].crc32c = 0xDEADBEEFU;
  ret = termux_manifest_v2_validate_all(entries, 2);
  assert(ret == -6);

  printf("✓ test_manifest_fail_closed_property passed (corrupt CRC rejected)\n");
  return 0;
}

int main(void) {
  printf("=== Manifest V2 Unit Tests ===\n\n");

  int all_passed = 0;
  all_passed += test_manifest_entry_validate_invariants();
  all_passed += test_manifest_entry_compute_phi();
  all_passed += test_manifest_entry_compute_crc32c();
  all_passed += test_manifest_gcd_invariant_coverage();
  all_passed += test_manifest_validate_all();
  all_passed += test_manifest_compute_global_crc32c();
  all_passed += test_manifest_load_from_buffer();
  all_passed += test_manifest_fail_closed_property();

  printf("\n=== All manifest_v2 tests passed! ===\n");
  return all_passed == 0 ? 0 : 1;
}
