#ifndef TERMUX_MANIFEST_H
#define TERMUX_MANIFEST_H

#include <stdint.h>
#include <stddef.h>

#define TERMUX_PKG_NAME_LEN 64
#define TERMUX_PKG_VERSION_LEN 32
#define TERMUX_MAX_DEPS 16
#define TERMUX_MAX_PHASES 8
#define TERMUX_MAX_ENV_VARS 256
#define TERMUX_MAX_BUILD_ARGS 512
#define TERMUX_MAX_OUTPUT_SIZE (16 * 1024 * 1024)
#define TERMUX_MAX_JOBS 8

#define TERMUX_ARCH_AARCH64 0
#define TERMUX_ARCH_ARM 1
#define TERMUX_ARCH_X86_64 2
#define TERMUX_ARCH_I686 3

#define TERMUX_PKG_KEEP_STATIC_LIBS 0x0001
#define TERMUX_PKG_NO_STATICALLY_LINKED_EXECUTABLES 0x0002
#define TERMUX_PKG_CLANG_ONLY 0x0004

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

struct termux_dep_node {
  uint16_t pkg_id;
  uint16_t num_deps;
  uint16_t dep_ids[TERMUX_MAX_DEPS];
};

struct termux_arch_flags {
  uint8_t arch;
  const char *arch_name;
  const char *llvm_target;
  const char *gnu_target;
  const char *cflags;
  const char *ldflags;
};

struct termux_lang_toolchain {
  const char *lang;
  const char *compiler_c;
  const char *compiler_cxx;
  const char *toolchain_setup;
};

struct termux_build_context {
  struct termux_pkg_manifest pkg;
  char build_output[TERMUX_MAX_OUTPUT_SIZE];
  char env_vars[TERMUX_MAX_ENV_VARS][256];
  char build_args[TERMUX_MAX_BUILD_ARGS][256];
  char output_dir[512];
  char source_dir[512];
  uint32_t output_pos;
  uint32_t output_size;
  uint32_t env_count;
  uint32_t arg_count;
  int job_pids[TERMUX_MAX_JOBS];
  uint8_t job_status[TERMUX_MAX_JOBS];
  uint8_t num_jobs;
  int exit_code;
  uint8_t phase_flags;
  uint8_t max_jobs;  // Maximum parallel jobs for build
};

typedef int (*termux_phase_fn)(struct termux_build_context *);

struct termux_phase_dispatch {
  const char *phase_name;
  termux_phase_fn handler;
};

int termux_load_manifest(const char *path);
void termux_unload_manifest(void);
const struct termux_pkg_manifest *termux_find_package(const char *pkg_name);
const struct termux_pkg_manifest *termux_find_package_by_arch(const char *pkg_name, uint8_t arch);
uint32_t termux_get_manifest_size(void);
const struct termux_pkg_manifest *termux_get_manifest_entry(uint32_t index);
const char *termux_get_string(uint32_t offset);
int termux_validate_manifest(void);
void termux_print_manifest_stats(void);

#endif
