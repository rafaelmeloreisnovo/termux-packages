#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define TERMUX_PKG_NAME_LEN 64
#define TERMUX_PKG_VERSION_LEN 32
#define TERMUX_MAX_DEPS 16

struct termux_pkg_manifest {
  char pkg_name[TERMUX_PKG_NAME_LEN];
  char version[TERMUX_PKG_VERSION_LEN];
  uint8_t arch;
  uint8_t api_level;
  uint16_t flags;
  uint32_t sha256[8];
  uint16_t num_deps;
  uint16_t num_phases;
  uint32_t source_url_offset;
  uint32_t patches_offset;
  uint32_t configure_args_offset;
  uint32_t custom_steps_offset;
  uint16_t dep_ids[TERMUX_MAX_DEPS];
};

static const char* arch_to_string(uint8_t arch) {
  switch (arch) {
    case 0: return "aarch64";
    case 1: return "arm";
    case 2: return "x86_64";
    case 3: return "i686";
    default: return "unknown";
  }
}

void dump_package(const struct termux_pkg_manifest *pkg, size_t idx) {
  printf("[%zu] Package: %s-%s\n", idx, pkg->pkg_name, pkg->version);
  printf("     Architecture: %s\n", arch_to_string(pkg->arch));
  printf("     API Level: %u\n", pkg->api_level);
  printf("     Flags: 0x%04x\n", pkg->flags);
  printf("     Dependencies: %u\n", pkg->num_deps);
  printf("     Phases: %u\n", pkg->num_phases);

  if (pkg->num_deps > 0) {
    printf("     Deps: [");
    for (uint16_t i = 0; i < pkg->num_deps && i < TERMUX_MAX_DEPS; i++) {
      if (i > 0) printf(", ");
      printf("%u", pkg->dep_ids[i]);
    }
    printf("]\n");
  }

  printf("     Offsets: source=%u patches=%u configure=%u custom=%u\n",
         pkg->source_url_offset, pkg->patches_offset,
         pkg->configure_args_offset, pkg->custom_steps_offset);
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <manifest.bin> [package_name]\n", argv[0]);
    fprintf(stderr, "  Without package_name: dumps all packages\n");
    fprintf(stderr, "  With package_name: dumps specific package\n");
    return 1;
  }

  const char *manifest_path = argv[1];
  const char *search_pkg = argc > 2 ? argv[2] : NULL;

  int fd = open(manifest_path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Error: Cannot open %s\n", manifest_path);
    return 1;
  }

  struct stat sb;
  if (fstat(fd, &sb) < 0) {
    fprintf(stderr, "Error: Cannot stat %s\n", manifest_path);
    close(fd);
    return 1;
  }

  size_t num_packages = (size_t)(sb.st_size / (off_t)sizeof(struct termux_pkg_manifest));
  printf("Manifest file: %s\n", manifest_path);
  printf("File size: %" PRIdMAX " bytes\n", (intmax_t)sb.st_size);
  printf("Packages: %zu\n\n", num_packages);

  struct termux_pkg_manifest pkg;
  size_t idx = 0;
  int found = 0;

  while (read(fd, &pkg, sizeof(pkg)) == sizeof(pkg)) {
    if (!search_pkg || strcmp(pkg.pkg_name, search_pkg) == 0) {
      dump_package(&pkg, idx);
      found++;
      if (search_pkg) break;
    }
    idx++;
  }

  if (search_pkg && !found) {
    fprintf(stderr, "Package not found: %s\n", search_pkg);
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}
