#include "manifest.h"
#include "friction_analyzer.h"
#include "checkpoint.h"
#include "parallel_jobs.h"
#include "build_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern ssize_t termux_write_file(const char *path, const char *buf, size_t buflen);
extern int termux_dir_create(const char *path);
extern int termux_dir_create_recursive(const char *path);

static const struct termux_arch_flags arch_flags[] = {
  { TERMUX_ARCH_AARCH64, "aarch64", "aarch64-linux-android", "aarch64-linux-gnu", "-march=armv8-a -O3", "-march=armv8-a" },
  { TERMUX_ARCH_ARM, "arm", "armv7a-linux-android", "arm-linux-gnueabihf", "-march=armv7-a -mfpu=neon -O3", "-march=armv7-a -mfpu=neon" },
  { TERMUX_ARCH_X86_64, "x86_64", "x86_64-linux-android", "x86_64-linux-gnu", "-march=x86-64 -O3", "-march=x86-64" },
  { TERMUX_ARCH_I686, "i686", "i686-linux-android", "i686-linux-gnu", "-march=i686 -O2", "-march=i686" },
};

static struct termux_build_context global_build_ctx;

static const struct termux_arch_flags *termux_arch_by_id(uint8_t arch) {
  for (size_t i = 0; i < sizeof(arch_flags) / sizeof(arch_flags[0]); i++) {
    if (arch_flags[i].arch == arch) return &arch_flags[i];
  }
  return NULL;
}

static int termux_arch_from_name(const char *name, uint8_t *out) {
  if (!name || !out) return -1;
  for (size_t i = 0; i < sizeof(arch_flags) / sizeof(arch_flags[0]); i++) {
    if (strcmp(name, arch_flags[i].arch_name) == 0) {
      *out = arch_flags[i].arch;
      return 0;
    }
  }
  return -1;
}

static int termux_append_output(struct termux_build_context *ctx, const char *msg) {
  if (!ctx || !msg) return -1;
  const size_t msg_len = strlen(msg);
  if (ctx->output_pos + msg_len >= ctx->output_size) return -1;
  memcpy(ctx->build_output + ctx->output_pos, msg, msg_len);
  ctx->output_pos += (uint32_t)msg_len;
  ctx->build_output[ctx->output_pos] = '\0';
  return 0;
}

static int termux_phase_setup_vars(struct termux_build_context *ctx) {
  const struct termux_arch_flags *flags = termux_arch_by_id(ctx->pkg.arch);
  if (!flags) return -1;

  char buf[1024];
  snprintf(buf, sizeof(buf),
           "[setup-vars] pkg=%s version=%s arch=%s api=%u jobs=%u\n"
           "TERMUX_HOST_PLATFORM=%s\nCFLAGS=%s\nLDFLAGS=%s\n",
           ctx->pkg.pkg_name, ctx->pkg.version, flags->arch_name,
           ctx->pkg.api_level, ctx->num_jobs, flags->llvm_target,
           flags->cflags, flags->ldflags);
  return termux_append_output(ctx, buf);
}

static int termux_phase_get_source(struct termux_build_context *ctx) {
  struct stat st;
  if (stat(ctx->source_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "[ERROR] TOKEN_VAZIO_SOURCE_DIR_NOT_MATERIALIZED path=%s\n",
             ctx->source_dir);
    termux_append_output(ctx, buf);
    return 72;
  }

  const char *source_url = termux_get_string(ctx->pkg.source_url_offset);
  char buf[1536];
  snprintf(buf, sizeof(buf),
           "[get-source] mode=PREMATERIALIZED path=%s manifest_url=%s\n",
           ctx->source_dir,
           source_url && source_url[0] ? source_url : "TOKEN_VAZIO_SOURCE_URL");
  return termux_append_output(ctx, buf);
}

static int termux_phase_apply_patches(struct termux_build_context *ctx) {
  const char *patches = termux_get_string(ctx->pkg.patches_offset);
  char buf[1024];
  snprintf(buf, sizeof(buf),
           "[apply-patches] state=%s descriptor=%s\n",
           patches && patches[0] ? "TOKEN_VAZIO_PATCH_EXECUTION" : "NO_PATCHES_DECLARED",
           patches && patches[0] ? patches : "none");
  if (termux_append_output(ctx, buf) != 0) return -1;
  return patches && patches[0] ? 73 : 0;
}

static int termux_phase_configure(struct termux_build_context *ctx) {
  char build_dir[512];
  if (snprintf(build_dir, sizeof(build_dir), "%s/build-%s",
               ctx->output_dir, ctx->pkg.pkg_name) >= (int)sizeof(build_dir)) {
    return -1;
  }
  if (termux_dir_create_recursive(build_dir) != 0) return -1;

  char buf[1024];
  snprintf(buf, sizeof(buf), "[configure] source=%s build=%s\n",
           ctx->source_dir, build_dir);
  if (termux_append_output(ctx, buf) != 0) return -1;

  const int ret = termux_exec_configure(ctx, ctx->source_dir, build_dir);
  if (ret != 0) {
    snprintf(buf, sizeof(buf), "[ERROR] configure failed code=%d\n", ret);
    termux_append_output(ctx, buf);
  }
  return ret;
}

static int termux_select_make_dir(struct termux_build_context *ctx,
                                  char *selected,
                                  size_t selected_size) {
  char build_dir[512];
  char path[1024];
  struct stat st;

  if (snprintf(build_dir, sizeof(build_dir), "%s/build-%s",
               ctx->output_dir, ctx->pkg.pkg_name) >= (int)sizeof(build_dir)) {
    return -1;
  }

  if (snprintf(path, sizeof(path), "%s/Makefile", build_dir) >= (int)sizeof(path)) {
    return -1;
  }
  if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
    return snprintf(selected, selected_size, "%s", build_dir) < (int)selected_size ? 0 : -1;
  }

  if (snprintf(path, sizeof(path), "%s/Makefile", ctx->source_dir) >= (int)sizeof(path)) {
    return -1;
  }
  if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
    return snprintf(selected, selected_size, "%s", ctx->source_dir) < (int)selected_size ? 0 : -1;
  }

  char buf[1200];
  snprintf(buf, sizeof(buf),
           "[ERROR] TOKEN_VAZIO_MAKEFILE_NOT_FOUND build=%s source=%s\n",
           build_dir, ctx->source_dir);
  termux_append_output(ctx, buf);
  return 75;
}

static int termux_phase_make(struct termux_build_context *ctx) {
  char make_dir[512];
  const int selected = termux_select_make_dir(ctx, make_dir, sizeof(make_dir));
  if (selected != 0) return selected;
  return termux_exec_make(ctx, make_dir, ctx->num_jobs);
}

static int termux_phase_install(struct termux_build_context *ctx) {
  char make_dir[512];
  char prefix_dir[512];
  const int selected = termux_select_make_dir(ctx, make_dir, sizeof(make_dir));
  if (selected != 0) return selected;
  if (snprintf(prefix_dir, sizeof(prefix_dir), "%s/prefix-%s",
               ctx->output_dir, ctx->pkg.pkg_name) >= (int)sizeof(prefix_dir)) {
    return -1;
  }
  if (termux_dir_create_recursive(prefix_dir) != 0) return -1;
  return termux_exec_make_install(ctx, make_dir, prefix_dir);
}

static int termux_phase_massage(struct termux_build_context *ctx) {
  return termux_append_output(ctx,
      "[massage] state=TOKEN_VAZIO_POST_PROCESSING_NOT_IMPLEMENTED no_mutation=true\n");
}

static int termux_phase_package(struct termux_build_context *ctx) {
  const struct termux_arch_flags *flags = termux_arch_by_id(ctx->pkg.arch);
  if (!flags) return -1;

  char prefix_dir[512];
  if (snprintf(prefix_dir, sizeof(prefix_dir), "%s/prefix-%s",
               ctx->output_dir, ctx->pkg.pkg_name) >= (int)sizeof(prefix_dir)) {
    return -1;
  }

  return termux_collect_artifacts(prefix_dir, ctx->output_dir,
                                  ctx->pkg.pkg_name, ctx->pkg.version,
                                  flags->arch_name);
}

static const struct termux_phase_dispatch phase_table[] = {
  { "setup-vars", termux_phase_setup_vars },
  { "get-source", termux_phase_get_source },
  { "apply-patches", termux_phase_apply_patches },
  { "configure", termux_phase_configure },
  { "make", termux_phase_make },
  { "install", termux_phase_install },
  { "massage", termux_phase_massage },
  { "package", termux_phase_package },
  { NULL, NULL },
};

static int termux_execute_build(struct termux_build_context *ctx) {
  ctx->output_pos = 0;
  ctx->exit_code = 0;
  ctx->build_output[0] = '\0';

  for (size_t i = 0; phase_table[i].phase_name; i++) {
    char msg[256];
    termux_metric_phase_start(phase_table[i].phase_name);
    snprintf(msg, sizeof(msg), "=== %s ===\n", phase_table[i].phase_name);
    if (termux_append_output(ctx, msg) != 0) return -1;

    const int ret = phase_table[i].handler(ctx);
    termux_metric_phase_end(phase_table[i].phase_name, ret);
    if (ret != 0) {
      snprintf(msg, sizeof(msg), "ERROR: %s failed code=%d\n",
               phase_table[i].phase_name, ret);
      termux_append_output(ctx, msg);
      ctx->exit_code = ret;
      return ret;
    }
    termux_checkpoint_record_phase((uint32_t)i);
    if (termux_append_output(ctx, "\n") != 0) return -1;
  }
  return 0;
}

static void print_usage(const char *prog) {
  fprintf(stderr,
      "Usage: %s --manifest <manifest.bin> --package <name> [options]\n"
      "Options:\n"
      "  --arch <aarch64|arm|x86_64|i686>  Override manifest default target\n"
      "  --api <21-34>                     Override manifest default API\n"
      "  --source-dir <path>                Pre-materialized source tree\n"
      "  --output <dir>                     Output directory (default ./build)\n"
      "  --jobs <1-8>                       Make jobs (default 1)\n"
      "  --metrics <file>                   Save friction report\n"
      "  --checkpoint <file>                Save final checkpoint\n"
      "  --resume <file>                    Rejected until replay semantics are validated\n",
      prog);
}

int main(int argc, char *const argv[]) {
  struct termux_build_context *ctx = &global_build_ctx;
  memset(ctx, 0, sizeof(*ctx));
  ctx->output_size = TERMUX_MAX_OUTPUT_SIZE;
  ctx->num_jobs = 1;

  const char *manifest_path = NULL;
  const char *package_name = NULL;
  const char *arch_name = NULL;
  const char *source_dir = NULL;
  const char *output_dir = "./build";
  const char *metrics_file = NULL;
  const char *checkpoint_file = NULL;
  const char *resume_file = NULL;
  int api_override = -1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
      manifest_path = argv[++i];
    } else if (strcmp(argv[i], "--package") == 0 && i + 1 < argc) {
      package_name = argv[++i];
    } else if (strcmp(argv[i], "--arch") == 0 && i + 1 < argc) {
      arch_name = argv[++i];
    } else if (strcmp(argv[i], "--api") == 0 && i + 1 < argc) {
      api_override = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--source-dir") == 0 && i + 1 < argc) {
      source_dir = argv[++i];
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
      if (jobs < 1 || jobs > TERMUX_MAX_JOBS) {
        fprintf(stderr, "Invalid jobs: %d\n", jobs);
        return 2;
      }
      ctx->num_jobs = (uint8_t)jobs;
    } else {
      fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
      print_usage(argv[0]);
      return 2;
    }
  }

  if (!manifest_path || !package_name) {
    print_usage(argv[0]);
    return 2;
  }

  if (resume_file) {
    fprintf(stderr,
            "TOKEN_VAZIO_RESUME_NOT_VALIDATED file=%s claim_allowed=false\n",
            resume_file);
    return 74;
  }

  if (termux_load_manifest(manifest_path) != 0 ||
      termux_validate_manifest() != 0) {
    termux_unload_manifest();
    return 3;
  }

  const struct termux_pkg_manifest *manifest_pkg = termux_find_package(package_name);
  if (!manifest_pkg) {
    fprintf(stderr, "Package not found in manifest: %s\n", package_name);
    termux_unload_manifest();
    return 4;
  }
  memcpy(&ctx->pkg, manifest_pkg, sizeof(ctx->pkg));
  ctx->pkg.pkg_name[TERMUX_PKG_NAME_LEN - 1] = '\0';
  ctx->pkg.version[TERMUX_PKG_VERSION_LEN - 1] = '\0';

  if (arch_name) {
    uint8_t arch = 0;
    if (termux_arch_from_name(arch_name, &arch) != 0) {
      fprintf(stderr, "Invalid architecture: %s\n", arch_name);
      termux_unload_manifest();
      return 5;
    }
    ctx->pkg.arch = arch;
  }

  if (api_override >= 0) {
    if (api_override < 21 || api_override > 34) {
      fprintf(stderr, "Invalid API level: %d\n", api_override);
      termux_unload_manifest();
      return 6;
    }
    ctx->pkg.api_level = (uint8_t)api_override;
  }

  if (snprintf(ctx->output_dir, sizeof(ctx->output_dir), "%s", output_dir) >=
      (int)sizeof(ctx->output_dir)) {
    termux_unload_manifest();
    return 7;
  }
  if (source_dir) {
    if (snprintf(ctx->source_dir, sizeof(ctx->source_dir), "%s", source_dir) >=
        (int)sizeof(ctx->source_dir)) {
      termux_unload_manifest();
      return 7;
    }
  } else if (snprintf(ctx->source_dir, sizeof(ctx->source_dir), "%s/build-%s",
                      output_dir, ctx->pkg.pkg_name) >= (int)sizeof(ctx->source_dir)) {
    termux_unload_manifest();
    return 7;
  }

  if (termux_dir_create_recursive(output_dir) != 0) {
    fprintf(stderr, "Failed to create output directory: %s\n", output_dir);
    termux_unload_manifest();
    return 8;
  }

  printf("manifest_gate=PASS package=%s version=%s arch=%u api=%u\n",
         ctx->pkg.pkg_name, ctx->pkg.version, ctx->pkg.arch, ctx->pkg.api_level);
  printf("source_mode=PREMATERIALIZED source_dir=%s\n", ctx->source_dir);
  printf("claim_allowed=false release_allowed=false\n\n");

  termux_reset_metrics();
  termux_checkpoint_reset();
  const int ret = termux_execute_build(ctx);

  printf("=== BUILD OUTPUT ===\n%s\n", ctx->build_output);
  printf(ret == 0 ? "[SUCCESS] Build completed\n" : "[FAILED] Build failed code=%d\n", ret);
  termux_print_metrics_summary();

  if (metrics_file && termux_write_metrics_report(metrics_file) != 0) {
    fprintf(stderr, "Failed to write metrics: %s\n", metrics_file);
  }
  if (checkpoint_file && ret == 0 &&
      termux_checkpoint_save(checkpoint_file, ctx, TERMUX_MAX_PHASES - 1) != 0) {
    fprintf(stderr, "Failed to save checkpoint: %s\n", checkpoint_file);
  }

  termux_unload_manifest();
  return ret;
}
