#include "manifest.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>

#define TERMUX_MANIFEST_MAGIC 0x5445524D  /* "TERM" */
#define TERMUX_MANIFEST_VERSION 1
#define TERMUX_ENTRY_SIZE 184u
#define TERMUX_HEADER_SIZE 20u
#define TERMUX_SIZE_FIELD_SIZE 4u

static uint8_t *manifest_data = NULL;
static size_t manifest_size = 0;
static uint32_t num_manifest_entries = 0;
static uint32_t string_pool_offset = 0;
static uint32_t string_pool_size = 0;

static uint32_t read_u32_le(const uint8_t *p) {
  return ((uint32_t)p[0]) |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static int checked_add_size(size_t a, size_t b, size_t *out) {
  if (a > SIZE_MAX - b) return -1;
  *out = a + b;
  return 0;
}

static int checked_mul_size(size_t a, size_t b, size_t *out) {
  if (a != 0 && b > SIZE_MAX / a) return -1;
  *out = a * b;
  return 0;
}

void termux_unload_manifest(void) {
  free(manifest_data);
  manifest_data = NULL;
  manifest_size = 0;
  num_manifest_entries = 0;
  string_pool_offset = 0;
  string_pool_size = 0;
}

int termux_load_manifest(const char *path) {
  if (!path || path[0] == '\0') {
    fprintf(stderr, "Manifest path is empty\n");
    return -1;
  }

  termux_unload_manifest();

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    perror("open manifest");
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    perror("fstat manifest");
    close(fd);
    return -1;
  }

  if (st.st_size < (off_t)(TERMUX_HEADER_SIZE + TERMUX_SIZE_FIELD_SIZE)) {
    fprintf(stderr, "Manifest too small: %lld bytes\n", (long long)st.st_size);
    close(fd);
    return -1;
  }

  manifest_data = (uint8_t *)malloc((size_t)st.st_size);
  if (!manifest_data) {
    perror("malloc manifest");
    close(fd);
    return -1;
  }

  size_t total_read = 0;
  while (total_read < (size_t)st.st_size) {
    ssize_t ret = read(fd, manifest_data + total_read,
                       (size_t)st.st_size - total_read);
    if (ret < 0) {
      perror("read manifest");
      close(fd);
      termux_unload_manifest();
      return -1;
    }
    if (ret == 0) break;
    total_read += (size_t)ret;
  }
  close(fd);

  if (total_read != (size_t)st.st_size) {
    fprintf(stderr, "Manifest short read: expected=%lld actual=%zu\n",
            (long long)st.st_size, total_read);
    termux_unload_manifest();
    return -1;
  }

  manifest_size = total_read;
  const uint32_t magic = read_u32_le(manifest_data + 0);
  const uint32_t version = read_u32_le(manifest_data + 4);
  num_manifest_entries = read_u32_le(manifest_data + 8);
  string_pool_offset = read_u32_le(manifest_data + 16);
  string_pool_size = read_u32_le(manifest_data + TERMUX_HEADER_SIZE);

  if (magic != TERMUX_MANIFEST_MAGIC) {
    fprintf(stderr, "Invalid manifest magic: 0x%08x\n", magic);
    termux_unload_manifest();
    return -1;
  }

  if (version != TERMUX_MANIFEST_VERSION) {
    fprintf(stderr, "Unsupported manifest version: %u\n", version);
    termux_unload_manifest();
    return -1;
  }

  size_t entries_bytes = 0;
  size_t entries_end = 0;
  size_t pool_end = 0;
  if (checked_mul_size((size_t)num_manifest_entries,
                       (size_t)TERMUX_ENTRY_SIZE,
                       &entries_bytes) != 0 ||
      checked_add_size((size_t)TERMUX_HEADER_SIZE + TERMUX_SIZE_FIELD_SIZE,
                       entries_bytes,
                       &entries_end) != 0 ||
      checked_add_size((size_t)string_pool_offset,
                       (size_t)string_pool_size,
                       &pool_end) != 0) {
    fprintf(stderr, "Manifest size arithmetic overflow\n");
    termux_unload_manifest();
    return -1;
  }

  if ((size_t)string_pool_offset != entries_end ||
      entries_end > manifest_size || pool_end > manifest_size) {
    fprintf(stderr,
            "Manifest layout invalid: entries_end=%zu pool_offset=%u pool_end=%zu size=%zu\n",
            entries_end, string_pool_offset, pool_end, manifest_size);
    termux_unload_manifest();
    return -1;
  }

  printf("Manifest loaded: %u packages\n", num_manifest_entries);
  return 0;
}

const struct termux_pkg_manifest *termux_get_manifest_entry(uint32_t index) {
  if (!manifest_data || index >= num_manifest_entries) return NULL;
  const size_t entry_base = TERMUX_HEADER_SIZE + TERMUX_SIZE_FIELD_SIZE;
  const size_t offset = entry_base + (size_t)index * TERMUX_ENTRY_SIZE;
  if (offset + TERMUX_ENTRY_SIZE > manifest_size) return NULL;
  return (const struct termux_pkg_manifest *)(manifest_data + offset);
}

const struct termux_pkg_manifest *termux_find_package(const char *pkg_name) {
  if (!manifest_data || !pkg_name) return NULL;
  for (uint32_t i = 0; i < num_manifest_entries; i++) {
    const struct termux_pkg_manifest *entry = termux_get_manifest_entry(i);
    if (!entry) return NULL;
    if (strncmp(entry->pkg_name, pkg_name, TERMUX_PKG_NAME_LEN) == 0) {
      return entry;
    }
  }
  return NULL;
}

uint32_t termux_get_manifest_size(void) {
  return num_manifest_entries;
}

const char *termux_get_string(uint32_t offset) {
  if (!manifest_data || offset >= string_pool_size) return NULL;
  const size_t absolute = (size_t)string_pool_offset + offset;
  const size_t remaining = (size_t)string_pool_size - offset;
  const char *value = (const char *)(manifest_data + absolute);
  if (!memchr(value, '\0', remaining)) return NULL;
  return value;
}

int termux_validate_manifest(void) {
  if (!manifest_data || num_manifest_entries == 0) {
    fprintf(stderr, "No manifest loaded\n");
    return -1;
  }

  for (uint32_t i = 0; i < num_manifest_entries; i++) {
    const struct termux_pkg_manifest *entry = termux_get_manifest_entry(i);
    if (!entry) {
      fprintf(stderr, "Manifest entry %u is out of bounds\n", i);
      return -1;
    }

    if (!memchr(entry->pkg_name, '\0', TERMUX_PKG_NAME_LEN) ||
        entry->pkg_name[0] == '\0') {
      fprintf(stderr, "Manifest entry %u has invalid package name\n", i);
      return -1;
    }

    if (!memchr(entry->version, '\0', TERMUX_PKG_VERSION_LEN) ||
        entry->version[0] == '\0') {
      fprintf(stderr, "Package %s has empty or unterminated version\n",
              entry->pkg_name);
      return -1;
    }

    if (entry->num_deps > TERMUX_MAX_DEPS) {
      fprintf(stderr, "Package %s has too many dependencies: %u\n",
              entry->pkg_name, entry->num_deps);
      return -1;
    }

    if (entry->arch > TERMUX_ARCH_I686) {
      fprintf(stderr, "Package %s has invalid arch: %u\n",
              entry->pkg_name, entry->arch);
      return -1;
    }

    if (entry->api_level < 21 || entry->api_level > 34) {
      fprintf(stderr, "Package %s has invalid API level: %u\n",
              entry->pkg_name, entry->api_level);
      return -1;
    }

    const uint32_t offsets[] = {
      entry->source_url_offset,
      entry->patches_offset,
      entry->configure_args_offset,
      entry->custom_steps_offset
    };
    for (size_t j = 0; j < sizeof(offsets) / sizeof(offsets[0]); j++) {
      if (offsets[j] != 0 && termux_get_string(offsets[j]) == NULL) {
        fprintf(stderr, "Package %s has invalid string offset: %u\n",
                entry->pkg_name, offsets[j]);
        return -1;
      }
    }
  }

  printf("Manifest validation passed (%u packages)\n", num_manifest_entries);
  return 0;
}

void termux_print_manifest_stats(void) {
  if (!manifest_data || num_manifest_entries == 0) {
    printf("No manifest loaded\n");
    return;
  }

  uint32_t arch_counts[4] = {0};
  uint32_t total_deps = 0;
  for (uint32_t i = 0; i < num_manifest_entries; i++) {
    const struct termux_pkg_manifest *entry = termux_get_manifest_entry(i);
    if (!entry) continue;
    if (entry->arch <= TERMUX_ARCH_I686) arch_counts[entry->arch]++;
    total_deps += entry->num_deps;
  }

  printf("=== Manifest Statistics ===\n");
  printf("Total packages: %u\n", num_manifest_entries);
  printf("Total dependencies: %u (avg: %.1f per pkg)\n",
         total_deps, (float)total_deps / num_manifest_entries);
  printf("Architectures:\n");
  printf("  aarch64: %u\n", arch_counts[TERMUX_ARCH_AARCH64]);
  printf("  arm: %u\n", arch_counts[TERMUX_ARCH_ARM]);
  printf("  x86_64: %u\n", arch_counts[TERMUX_ARCH_X86_64]);
  printf("  i686: %u\n", arch_counts[TERMUX_ARCH_I686]);
  printf("String pool size: %u bytes\n", string_pool_size);
}
