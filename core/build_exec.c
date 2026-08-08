#include "build_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

extern ssize_t termux_read_file(const char *path, char *buf, size_t buflen);
extern int termux_execve_capture(const char *path, char *const argv[], char *const envp[],
                                  char *output_buf, size_t output_size, size_t *output_len);

/*
 * Build-time ABI/filesystem invariant for the RAFCODEΦ fork.
 *
 * This is deliberately distinct from DESTDIR. --prefix is the runtime path
 * compiled into packages; DESTDIR is only a host-side staging root. Replacing
 * the runtime prefix after compilation is forbidden because ELF/data payloads
 * may embed it and the RAFCODEΦ prefix has a different length from upstream.
 */
#define RAFCODEPHI_TARGET_PREFIX "/data/data/com.termux.rafacodephi/files/usr"

static int termux_make_absolute_path(const char *path, char *abs_path, size_t max_len) {
  if (!path || !abs_path || max_len == 0) return -1;
  if (path[0] == '/') {
    if (snprintf(abs_path, max_len, "%s", path) >= (int)max_len) return -1;
    return 0;
  }

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) == NULL) return -1;
  if (snprintf(abs_path, max_len, "%s/%s", cwd, path) >= (int)max_len) return -1;
  return 0;
}

static const char *termux_resolve_shell(void) {
  const char *configured = getenv("TERMUX_BUILD_SHELL");
  if (configured && configured[0] == '/' && access(configured, X_OK) == 0) {
    return configured;
  }

  const char *prefix = getenv("PREFIX");
  static char prefix_shell[PATH_MAX];
  if (prefix && prefix[0] == '/') {
    if (snprintf(prefix_shell, sizeof(prefix_shell), "%s/bin/sh", prefix) <
        (int)sizeof(prefix_shell) && access(prefix_shell, X_OK) == 0) {
      return prefix_shell;
    }
  }

  if (access("/bin/sh", X_OK) == 0) return "/bin/sh";
  return NULL;
}

static int termux_capture_append(struct termux_build_context *ctx,
                                 const char *label,
                                 const char *output,
                                 size_t output_len) {
  if (!ctx || !output || output_len == 0) return 0;

  char header[128];
  int header_len = snprintf(header, sizeof(header), "[%s] Output (%zu bytes):\n",
                            label ? label : "exec", output_len);
  if (header_len < 0) return -1;

  const size_t needed = (size_t)header_len + output_len;
  if (ctx->output_pos + needed >= ctx->output_size) return -1;

  memcpy(ctx->build_output + ctx->output_pos, header, (size_t)header_len);
  ctx->output_pos += (uint32_t)header_len;
  memcpy(ctx->build_output + ctx->output_pos, output, output_len);
  ctx->output_pos += (uint32_t)output_len;
  return 0;
}

int termux_exec_configure(struct termux_build_context *ctx,
                          const char *source_dir,
                          const char *build_dir) {
  if (!ctx || !source_dir || !build_dir) return -1;

  const char *shell = termux_resolve_shell();
  if (!shell) return -1;

  char abs_source[512];
  char abs_build[512];
  if (termux_make_absolute_path(source_dir, abs_source, sizeof(abs_source)) != 0 ||
      termux_make_absolute_path(build_dir, abs_build, sizeof(abs_build)) != 0) {
    return -1;
  }

  const char prefix_contract[] =
      "target_prefix=" RAFCODEPHI_TARGET_PREFIX
      " staging_model=DESTDIR claim_allowed=false\n";
  if (termux_capture_append(ctx, "configure-contract", prefix_contract,
                            sizeof(prefix_contract) - 1) != 0) {
    return -1;
  }

  char configure_cmd[1536];
  int n = snprintf(configure_cmd, sizeof(configure_cmd),
      "cd '%s' || exit 70; "
      "if [ -x '%s/configure' ]; then "
      "  '%s/configure' --prefix='%s'; "
      "elif [ -x ./configure ]; then "
      "  ./configure --prefix='%s'; "
      "else "
      "  printf 'TOKEN_VAZIO_NO_CONFIGURE_SCRIPT\\n'; "
      "fi",
      abs_build, abs_source, abs_source,
      RAFCODEPHI_TARGET_PREFIX, RAFCODEPHI_TARGET_PREFIX);
  if (n < 0 || n >= (int)sizeof(configure_cmd)) return -1;

  char *argv[] = { (char *)shell, "-c", configure_cmd, NULL };
  char output[8192];
  size_t output_len = 0;
  int ret = termux_execve_capture(shell, argv, NULL,
                                  output, sizeof(output), &output_len);
  if (termux_capture_append(ctx, "configure", output, output_len) != 0) return -1;
  return ret;
}

int termux_exec_make(struct termux_build_context *ctx,
                     const char *build_dir,
                     uint32_t num_jobs) {
  if (!ctx || !build_dir) return -1;
  if (num_jobs == 0) num_jobs = 1;
  if (num_jobs > TERMUX_MAX_JOBS) num_jobs = TERMUX_MAX_JOBS;

  const char *shell = termux_resolve_shell();
  if (!shell) return -1;

  char abs_build[512];
  if (termux_make_absolute_path(build_dir, abs_build, sizeof(abs_build)) != 0) {
    return -1;
  }

  char make_cmd[1024];
  int n = snprintf(make_cmd, sizeof(make_cmd),
                   "cd '%s' || exit 70; make -j%u",
                   abs_build, num_jobs);
  if (n < 0 || n >= (int)sizeof(make_cmd)) return -1;

  char *argv[] = { (char *)shell, "-c", make_cmd, NULL };
  char output[16384];
  size_t output_len = 0;
  int ret = termux_execve_capture(shell, argv, NULL,
                                  output, sizeof(output), &output_len);
  if (termux_capture_append(ctx, "make", output, output_len) != 0) return -1;
  return ret;
}

int termux_exec_make_install(struct termux_build_context *ctx,
                             const char *build_dir,
                             const char *prefix_dir) {
  if (!ctx || !build_dir || !prefix_dir) return -1;

  const char *shell = termux_resolve_shell();
  if (!shell) return -1;

  char abs_build[512];
  char abs_prefix[512];
  if (termux_make_absolute_path(build_dir, abs_build, sizeof(abs_build)) != 0 ||
      termux_make_absolute_path(prefix_dir, abs_prefix, sizeof(abs_prefix)) != 0) {
    return -1;
  }

  char install_cmd[1536];
  int n = snprintf(install_cmd, sizeof(install_cmd),
                   "cd '%s' || exit 70; make install DESTDIR='%s'",
                   abs_build, abs_prefix);
  if (n < 0 || n >= (int)sizeof(install_cmd)) return -1;

  char *argv[] = { (char *)shell, "-c", install_cmd, NULL };
  char output[16384];
  size_t output_len = 0;
  int ret = termux_execve_capture(shell, argv, NULL,
                                  output, sizeof(output), &output_len);
  if (termux_capture_append(ctx, "install", output, output_len) != 0) return -1;
  return ret;
}

int termux_collect_artifacts(const char *prefix_dir,
                             const char *output_dir,
                             const char *pkg_name,
                             const char *version,
                             const char *arch_name) {
  if (!prefix_dir || !output_dir || !pkg_name || !version || !arch_name ||
      pkg_name[0] == '\0' || version[0] == '\0') {
    return -1;
  }

  const char *shell = termux_resolve_shell();
  if (!shell) return -1;

  char abs_prefix[512];
  char abs_output[512];
  if (termux_make_absolute_path(prefix_dir, abs_prefix, sizeof(abs_prefix)) != 0 ||
      termux_make_absolute_path(output_dir, abs_output, sizeof(abs_output)) != 0) {
    return -1;
  }

  char tar_path[1024];
  char tar_cmd[2048];
  int n = snprintf(tar_path, sizeof(tar_path), "%s/%s-%s-%s.tar.gz",
                   abs_output, pkg_name, version, arch_name);
  if (n < 0 || n >= (int)sizeof(tar_path)) return -1;

  n = snprintf(tar_cmd, sizeof(tar_cmd),
               "cd '%s' || exit 70; "
               "test -n \"$(find . -mindepth 1 -print -quit)\" || exit 71; "
               "tar -czf '%s' .",
               abs_prefix, tar_path);
  if (n < 0 || n >= (int)sizeof(tar_cmd)) return -1;

  char *argv[] = { (char *)shell, "-c", tar_cmd, NULL };
  char output[4096];
  size_t output_len = 0;
  return termux_execve_capture(shell, argv, NULL,
                               output, sizeof(output), &output_len);
}
