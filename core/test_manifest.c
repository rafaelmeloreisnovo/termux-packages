#include "manifest.h"
#include <stdio.h>

extern int termux_load_manifest(const char *path);
extern uint32_t termux_get_manifest_size(void);
extern const struct termux_pkg_manifest *termux_get_manifest_entry(uint32_t index);
extern const struct termux_pkg_manifest *termux_find_package(const char *pkg_name);
extern int termux_validate_manifest(void);
extern void termux_print_manifest_stats(void);

int main(void) {
  printf("Loading manifest...\n");
  if (termux_load_manifest("manifest.bin") != 0) {
    fprintf(stderr, "Failed to load manifest\n");
    return 1;
  }

  printf("✓ Manifest loaded\n\n");

  termux_validate_manifest();
  printf("\n");

  termux_print_manifest_stats();
  printf("\n");

  uint32_t size = termux_get_manifest_size();
  if (size > 0) {
    const struct termux_pkg_manifest *pkg = termux_get_manifest_entry(0);
    printf("First package: %s-%s\n", pkg->pkg_name, "?");
  }

  const struct termux_pkg_manifest *vim = termux_find_package("vim");
  if (vim) {
    printf("✓ Found vim package (arch=%u, api=%u)\n", vim->arch, vim->api_level);
  } else {
    printf("✗ vim package not found\n");
  }

  return 0;
}
