#include "manifest.h"
#include "friction_analyzer.h"
#include "checkpoint.h"
#include "parallel_jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern ssize_t termux_read_file(const char *path, char *buf, size_t buflen);
extern ssize_t termux_write_file(const char *path, const char *buf, size_t buflen);
extern ssize_t termux_append_file(const char *path, const char *buf, size_t buflen);
extern int termux_execve_capture(const char *path, char *const argv[], char *const envp[],
                                  char *output_buf, size_t output_size, size_t *output_len);
extern pid_t termux_spawn_job(const char *path, char *const argv[], char *const envp[]);
extern int termux_wait_job(pid_t pid);
extern int termux_path_exists(const char *path);
extern int termux_file_is_readable(const char *path);
extern int termux_dir_create(const char *path);
extern int termux_dir_create_recursive(const char *path);
extern int termux_create_tar(const char *output_path, const char *pkg_name,
                             const char *version, const char *arch,
                             const char *build_output);

static const struct termux_arch_flags arch_flags[] = {
  {
    .arch = TERMUX_ARCH_AARCH64,
    .arch_name = "aarch64",
    .llvm_target = "aarch64-linux-android",
    .gnu_target = "aarch64-linux-gnu",
    .cflags = "-march=armv8-a -O3",
    .ldflags = "-march=armv8-a"
  },
  {
    .arch = TERMUX_ARCH_ARM,
    .arch_name = "arm",
    .llvm_target = "armv7a-linux-android",
    .gnu_target = "arm-linux-gnueabihf",
    .cflags = "-march=armv7-a -mfpu=neon -O3",
    .ldflags = "-march=armv7-a -mfpu=neon"
  },
  {
    .arch = TERMUX_ARCH_X86_64,
    .arch_name = "x86_64",
    .llvm_target = "x86_64-linux-android",
    .gnu_target = "x86_64-linux-gnu",
    .cflags = "-march=x86-64 -O3",
    .ldflags = "-march=x86-64"
  },
  {
    .arch = TERMUX_ARCH_I686,
    .arch_name = "i686",
    .llvm_target = "i686-linux-android",
    .gnu_target = "i686-linux-gnu",
    .cflags = "-march=i686 -O2",
    .ldflags = "-march=i686"
  },
};

static const struct termux_lang_toolchain lang_toolchains[] __attribute__((unused)) = {
  { "c", "gcc", "g++", "none" },
  { "cpp", "gcc", "g++", "none" },
  { "go", "gcc", "g++", "golang" },
  { "rust", "rustc", "rustc", "rust" },
  { "python", "python3", "python3", "python" },
};

static int termux_append_output(struct termux_build_context *ctx, const char *msg) {
  if (!msg) return 0;

  size_t msg_len = strlen(msg);
  if (ctx->output_pos + msg_len >= ctx->output_size) {
    return -1;
  }

  memcpy(ctx->build_output + ctx->output_pos, msg, msg_len);
  ctx->output_pos += msg_len;
  return 0;
}

static int termux_phase_setup_vars(struct termux_build_context *ctx) {
  char buf[512];
  const struct termux_arch_flags *flags = NULL;

  for (size_t i = 0; i < sizeof(arch_flags) / sizeof(arch_flags[0]); i++) {
    if (arch_flags[i].arch == ctx->pkg.arch) {
      flags = &arch_flags[i];
      break;
    }
  }

  if (!flags) {
    return -1;
  }

  snprintf(buf, sizeof(buf), "[setup-vars] pkg=%s arch=%s api=%u\n",
           ctx->pkg.pkg_name, flags->arch_name, ctx->pkg.api_level);
  termux_append_output(ctx, buf);

  snprintf(buf, sizeof(buf), "export TERMUX_ARCH=%s\n", flags->arch_name);
  termux_append_output(ctx, buf);

  snprintf(buf, sizeof(buf), "export TERMUX_HOST_PLATFORM=%s\n", flags->llvm_target);
  termux_append_output(ctx, buf);

  snprintf(buf, sizeof(buf), "export TERMUX_PKG_API_LEVEL=%u\n", ctx->pkg.api_level);
  termux_append_output(ctx, buf);

  return 0;
}

static int termux_phase_get_source(struct termux_build_context *ctx) {
  char buf[512];

  snprintf(buf, sizeof(buf), "[get-source] Fetching %s-%s\n",
           ctx->pkg.pkg_name, ctx->pkg.version);
  termux_append_output(ctx, buf);

  return 0;
}

static int termux_phase_apply_patches(struct termux_build_context *ctx) {
  char buf[512];

  snprintf(buf, sizeof(buf), "[apply-patches] Processing patches for %s\n",
           ctx->pkg.pkg_name);
  termux_append_output(ctx, buf);

  return 0;
}

static int termux_phase_configure(struct termux_build_context *ctx) {
  char buf[512];

  snprintf(buf, sizeof(buf), "[configure] Configuring %s-%s\n",
           ctx->pkg.pkg_name, ctx->pkg.version);
  termux_append_output(ctx, buf);

  return 0;
}

static int termux_phase_make(struct termux_build_context *ctx) {
  char buf[512];

  snprintf(buf, sizeof(buf), "[make] Building %s\n", ctx->pkg.pkg_name);
  termux_append_output(ctx, buf);

  return 0;
}

static int termux_phase_massage(struct termux_build_context *ctx) {
  char buf[512];

  snprintf(buf, sizeof(buf), "[massage] Post-processing %s\n", ctx->pkg.pkg_name);
  termux_append_output(ctx, buf);

  return 0;
}

static int termux_phase_package(struct termux_build_context *ctx) {
  char buf[512];
  char output_path[512];
  const char *arch_name = "unknown";

  switch (ctx->pkg.arch) {
    case TERMUX_ARCH_AARCH64: arch_name = "aarch64"; break;
    case TERMUX_ARCH_ARM: arch_name = "arm"; break;
    case TERMUX_ARCH_X86_64: arch_name = "x86_64"; break;
    case TERMUX_ARCH_I686: arch_name = "i686"; break;
  }

  snprintf(buf, sizeof(buf), "[package] Creating tarball for %s-%s\n",
           ctx->pkg.pkg_name, ctx->pkg.version);
  termux_append_output(ctx, buf);

  snprintf(output_path, sizeof(output_path), "%s/%s-%s-%s.tar",
           ctx->output_dir, ctx->pkg.pkg_name, ctx->pkg.version, arch_name);

  if (termux_create_tar(output_path, ctx->pkg.pkg_name, ctx->pkg.version,
                        arch_name, ctx->build_output) != 0) {
    snprintf(buf, sizeof(buf), "[ERROR] Failed to create tarball: %s\n", output_path);
    termux_append_output(ctx, buf);
    return -1;
  }

  snprintf(buf, sizeof(buf), "[SUCCESS] Tarball created: %s\n", output_path);
  termux_append_output(ctx, buf);

  return 0;
}

static const struct termux_phase_dispatch phase_table[] = {
  { "setup-vars", termux_phase_setup_vars },
  { "get-source", termux_phase_get_source },
  { "apply-patches", termux_phase_apply_patches },
  { "configure", termux_phase_configure },
  { "make", termux_phase_make },
  { "massage", termux_phase_massage },
  { "package", termux_phase_package },
  { NULL, NULL },
};

static struct termux_build_context global_build_ctx;

int termux_execute_build(struct termux_build_context *ctx) {
  ctx->output_pos = 0;
  ctx->exit_code = 0;

  for (size_t i = 0; phase_table[i].phase_name; i++) {
    char msg[256];

    termux_metric_phase_start(phase_table[i].phase_name);

    snprintf(msg, sizeof(msg), "=== %s ===\n", phase_table[i].phase_name);
    termux_append_output(ctx, msg);

    int ret = phase_table[i].handler(ctx);
    termux_metric_phase_end(phase_table[i].phase_name, ret);

    if (ret != 0) {
      snprintf(msg, sizeof(msg), "ERROR: %s failed with code %d\n",
               phase_table[i].phase_name, ret);
      termux_append_output(ctx, msg);
      ctx->exit_code = ret;
      return ret;
    }

    termux_append_output(ctx, "\n");
  }

  return 0;
}

static void print_usage(const char *prog) {
  fprintf(stderr, "Usage: %s --manifest <manifest.bin> --package <name> --arch <arch> --api <level> [options]\n", prog);
  fprintf(stderr, "Required:\n");
  fprintf(stderr, "  --manifest <path>     Binary manifest file\n");
  fprintf(stderr, "  --package <name>      Package name\n");
  fprintf(stderr, "  --arch <arch>         Architecture: aarch64, arm, x86_64, i686\n");
  fprintf(stderr, "  --api <level>         API level: 21-34\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --output <dir>        Output directory (default: ./build)\n");
  fprintf(stderr, "  --metrics <file>      Save friction analysis report\n");
  fprintf(stderr, "  --checkpoint <file>   Save checkpoint for resumable builds\n");
  fprintf(stderr, "  --resume <file>       Resume from checkpoint file\n");
  fprintf(stderr, "  --jobs <num>          Parallel jobs (1-8, default: 1)\n");
}

int main(int argc, char *const argv[]) {
  struct termux_build_context *ctx = &global_build_ctx;
  memset(ctx, 0, sizeof(*ctx));
  ctx->output_size = TERMUX_MAX_OUTPUT_SIZE;
  ctx->pkg.api_level = 24;

  const char *manifest_path = NULL;
  const char *package_name = NULL;
  const char *arch_name = NULL;
  const char *output_dir = "./build";
  const char *metrics_file = NULL;
  const char *checkpoint_file = NULL;
  const char *resume_file = NULL;
  uint8_t max_jobs = 1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
      manifest_path = argv[++i];
    } else if (strcmp(argv[i], "--package") == 0 && i + 1 < argc) {
      package_name = argv[++i];
    } else if (strcmp(argv[i], "--arch") == 0 && i + 1 < argc) {
      arch_name = argv[++i];
    } else if (strcmp(argv[i], "--api") == 0 && i + 1 < argc) {
      ctx->pkg.api_level = (uint8_t)atoi(argv[++i]);
    } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
      output_dir = argv[++i];
    } else if (strcmp(argv[i], "--metrics") == 0 && i + 1 < argc) {
      metrics_file = argv[++i];
    } else if (strcmp(argv[i], "--checkpoint") == 0 && i + 1 < argc) {
      checkpoint_file = argv[++i];
    } else if (strcmp(argv[i], "--resume") == 0 && i + 1 < argc) {
      resume_file = argv[++i];
    } else if (strcmp(argv[i], "--jobs") == 0 && i + 1 < argc) {
      int jobs = atoi(argv[++i]);
      max_jobs = jobs > TERMUX_MAX_JOBS ? TERMUX_MAX_JOBS : (uint8_t)jobs;
    }
  }

  if (!manifest_path || !package_name || !arch_name) {
    print_usage(argv[0]);
    return 1;
  }

  strncpy(ctx->pkg.pkg_name, package_name, TERMUX_PKG_NAME_LEN - 1);

  if (strcmp(arch_name, "aarch64") == 0) {
    ctx->pkg.arch = TERMUX_ARCH_AARCH64;
  } else if (strcmp(arch_name, "arm") == 0) {
    ctx->pkg.arch = TERMUX_ARCH_ARM;
  } else if (strcmp(arch_name, "x86_64") == 0) {
    ctx->pkg.arch = TERMUX_ARCH_X86_64;
  } else if (strcmp(arch_name, "i686") == 0) {
    ctx->pkg.arch = TERMUX_ARCH_I686;
  } else {
    fprintf(stderr, "Invalid architecture: %s\n", arch_name);
    return 1;
  }

  if (ctx->pkg.api_level < 21 || ctx->pkg.api_level > 34) {
    fprintf(stderr, "Invalid API level: %u (range 21-34)\n", ctx->pkg.api_level);
    return 1;
  }

  printf("Building %s for %s (API %u)\n", ctx->pkg.pkg_name, arch_name, ctx->pkg.api_level);
  printf("Manifest: %s\n", manifest_path);
  printf("Output: %s\n\n", output_dir);

  strncpy(ctx->output_dir, output_dir, sizeof(ctx->output_dir) - 1);

  if (termux_dir_create_recursive(output_dir) != 0) {
    fprintf(stderr, "Failed to create output directory: %s\n", output_dir);
    return 1;
  }

  int ret = termux_execute_build(ctx);

  printf("=== BUILD OUTPUT ===\n%s\n", ctx->build_output);

  if (ret == 0) {
    printf("\n[SUCCESS] Build completed\n");
  } else {
    printf("\n[FAILED] Build failed with code %d\n", ret);
  }

  termux_print_metrics_summary();

  if (metrics_file) {
    if (termux_write_metrics_report(metrics_file) == 0) {
      printf("Friction metrics saved to: %s\n", metrics_file);
    }
  }

  if (checkpoint_file && ret == 0) {
    if (termux_checkpoint_save(checkpoint_file, ctx, TERMUX_MAX_PHASES - 1) == 0) {
      printf("Checkpoint saved to: %s\n", checkpoint_file);
    }
  }

  return ret;
}
