#include "manifest.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#define TERMUX_MANIFEST_MAGIC 0x5445524D  /* "TERM" */
#define TERMUX_MANIFEST_VERSION 1
#define TERMUX_ENTRY_SIZE 184
#define TERMUX_HEADER_SIZE 20

static uint8_t *manifest_data = NULL;
static size_t manifest_size = 0;
static uint32_t num_manifest_entries = 0;
static uint32_t string_pool_offset = 0;
static uint32_t string_pool_size = 0;

int termux_load_manifest(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    perror("fstat");
    close(fd);
    return -1;
  }

  manifest_data = (uint8_t *)malloc(st.st_size);
  if (!manifest_data) {
    perror("malloc");
    close(fd);
    return -1;
  }

  size_t total_read = 0;
  while (total_read < (size_t)st.st_size) {
    ssize_t ret = read(fd, manifest_data + total_read, st.st_size - total_read);
    if (ret < 0) {
      perror("read");
      free(manifest_data);
      manifest_data = NULL;
      close(fd);
      return -1;
    }
    if (ret == 0) break;
    total_read += ret;
  }
  close(fd);

  manifest_size = total_read;

  uint32_t *header = (uint32_t *)manifest_data;
  uint32_t magic = header[0];
  uint32_t version = header[1];
  num_manifest_entries = header[2];
  string_pool_offset = header[4];
  uint32_t *size_field = (uint32_t *)(manifest_data + TERMUX_HEADER_SIZE);
  string_pool_size = *size_field;

  if (magic != TERMUX_MANIFEST_MAGIC) {
    fprintf(stderr, "Invalid manifest magic: 0x%x\n", magic);
    free(manifest_data);
    manifest_data = NULL;
    return -1;
  }

  if (version != TERMUX_MANIFEST_VERSION) {
    fprintf(stderr, "Unsupported manifest version: %u\n", version);
    free(manifest_data);
    manifest_data = NULL;
    return -1;
  }

  printf("✓ Manifest loaded: %u packages\n", num_manifest_entries);
  return 0;
}

const struct termux_pkg_manifest *termux_find_package(const char *pkg_name) {
  if (!manifest_data) return NULL;

  uint8_t *entry_ptr = manifest_data + TERMUX_HEADER_SIZE + sizeof(uint32_t);

  for (uint32_t i = 0; i < num_manifest_entries; i++) {
    struct termux_pkg_manifest *entry = (struct termux_pkg_manifest *)entry_ptr;
    if (strcmp(entry->pkg_name, pkg_name) == 0) {
      return entry;
    }
    entry_ptr += TERMUX_ENTRY_SIZE;
  }
  return NULL;
}

const struct termux_pkg_manifest *termux_find_package_by_arch(const char *pkg_name, uint8_t arch) {
  if (!manifest_data) return NULL;

  uint8_t *entry_ptr = manifest_data + TERMUX_HEADER_SIZE + sizeof(uint32_t);

  for (uint32_t i = 0; i < num_manifest_entries; i++) {
    struct termux_pkg_manifest *entry = (struct termux_pkg_manifest *)entry_ptr;
    if (strcmp(entry->pkg_name, pkg_name) == 0 && entry->arch == arch) {
      return entry;
    }
    entry_ptr += TERMUX_ENTRY_SIZE;
  }
  return NULL;
}

uint32_t termux_get_manifest_size(void) {
  return num_manifest_entries;
}

const struct termux_pkg_manifest *termux_get_manifest_entry(uint32_t index) {
  if (!manifest_data || index >= num_manifest_entries) {
    return NULL;
  }

  uint8_t *entry_ptr = manifest_data + TERMUX_HEADER_SIZE + sizeof(uint32_t);
  entry_ptr += index * TERMUX_ENTRY_SIZE;
  return (struct termux_pkg_manifest *)entry_ptr;
}

const char *termux_get_string(uint32_t offset) {
  if (!manifest_data || offset >= string_pool_size) {
    return NULL;
  }
  return (const char *)(manifest_data + string_pool_offset + offset);
}

int termux_validate_manifest(void) {
  if (!manifest_data || num_manifest_entries == 0) {
    fprintf(stderr, "No manifest loaded\n");
    return -1;
  }

  for (uint32_t i = 0; i < num_manifest_entries; i++) {
    const struct termux_pkg_manifest *entry = termux_get_manifest_entry(i);
    if (!entry) continue;

    if (entry->num_deps > TERMUX_MAX_DEPS) {
      fprintf(stderr, "Package %s has too many dependencies: %u\n",
              entry->pkg_name, entry->num_deps);
      return -1;
    }

    if (entry->arch > 3) {
      fprintf(stderr, "Package %s has invalid arch: %u\n",
              entry->pkg_name, entry->arch);
      return -1;
    }

    if (entry->api_level < 21 || entry->api_level > 34) {
      fprintf(stderr, "Package %s has invalid API level: %u\n",
              entry->pkg_name, entry->api_level);
      return -1;
    }
  }

  printf("✓ Manifest validation passed (%u packages)\n", num_manifest_entries);
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
    arch_counts[entry->arch]++;
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
