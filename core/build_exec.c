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

static void termux_make_absolute_path(const char *path, char *abs_path, size_t max_len) {
  if (path[0] == '/') {
    strncpy(abs_path, path, max_len - 1);
    abs_path[max_len - 1] = '\0';
  } else {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      strncpy(abs_path, path, max_len - 1);
      abs_path[max_len - 1] = '\0';
      return;
    }
    snprintf(abs_path, max_len, "%s/%s", cwd, path);
  }
}

int termux_exec_configure(struct termux_build_context *ctx,
                          const char *source_dir,
                          const char *build_dir) {
  if (!ctx || !source_dir || !build_dir) {
    return -1;
  }

  char abs_source[512];
  char abs_build[512];
  termux_make_absolute_path(source_dir, abs_source, sizeof(abs_source));
  termux_make_absolute_path(build_dir, abs_build, sizeof(abs_build));

  char configure_cmd[1024];
  snprintf(configure_cmd, sizeof(configure_cmd),
           "cd '%s' && [ -x ./configure ] && ./configure --prefix='%s' 2>&1 || echo 'No configure script'",
           abs_source, abs_build);

  char *argv[] = { "/bin/bash", "-c", configure_cmd, NULL };
  char output[8192];
  size_t output_len = 0;
  int ret = termux_execve_capture("/bin/bash", argv, NULL, output, sizeof(output), &output_len);

  if (output_len > 0 && ctx->output_pos + output_len + 32 < ctx->output_size) {
    char buf[512];
    snprintf(buf, sizeof(buf), "[configure] Output (%zu bytes):\n", output_len);
    memcpy(ctx->build_output + ctx->output_pos, buf, strlen(buf));
    ctx->output_pos += strlen(buf);
    memcpy(ctx->build_output + ctx->output_pos, output, output_len);
    ctx->output_pos += output_len;
  }

  return ret;
}

int termux_exec_make(struct termux_build_context *ctx,
                     const char *build_dir,
                     uint32_t num_jobs) {
  if (!ctx || !build_dir) {
    return -1;
  }

  if (num_jobs == 0) {
    num_jobs = 1;
  }

  char abs_build[512];
  termux_make_absolute_path(build_dir, abs_build, sizeof(abs_build));

  char make_cmd[512];
  snprintf(make_cmd, sizeof(make_cmd),
           "cd '%s' && export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin && make -j%u 2>&1",
           abs_build, num_jobs);

  char *argv[] = { "/bin/bash", "-c", make_cmd, NULL };
  char output[16384];
  size_t output_len = 0;
  int ret = termux_execve_capture("/bin/bash", argv, NULL, output, sizeof(output), &output_len);

  if (output_len > 0 && ctx->output_pos + output_len < ctx->output_size) {
    memcpy(ctx->build_output + ctx->output_pos, output, output_len);
    ctx->output_pos += output_len;
  }

  return ret;
}

int termux_exec_make_install(struct termux_build_context *ctx,
                             const char *build_dir,
                             const char *prefix_dir) {
  if (!ctx || !build_dir || !prefix_dir) {
    return -1;
  }

  char abs_build[512];
  char abs_prefix[512];
  termux_make_absolute_path(build_dir, abs_build, sizeof(abs_build));
  termux_make_absolute_path(prefix_dir, abs_prefix, sizeof(abs_prefix));

  char install_cmd[1024];
  snprintf(install_cmd, sizeof(install_cmd),
           "cd '%s' && export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin && make install DESTDIR='%s' 2>&1",
           abs_build, abs_prefix);

  char *argv[] = { "/bin/bash", "-c", install_cmd, NULL };
  char output[16384];
  size_t output_len = 0;
  int ret = termux_execve_capture("/bin/bash", argv, NULL, output, sizeof(output), &output_len);

  if (output_len > 0 && ctx->output_pos + output_len < ctx->output_size) {
    memcpy(ctx->build_output + ctx->output_pos, output, output_len);
    ctx->output_pos += output_len;
  }

  return ret;
}

int termux_collect_artifacts(const char *prefix_dir,
                             const char *output_dir,
                             const char *pkg_name,
                             const char *version,
                             const char *arch_name) {
  if (!prefix_dir || !output_dir || !pkg_name || !arch_name) {
    return -1;
  }

  char abs_prefix[512];
  char abs_output[512];
  termux_make_absolute_path(prefix_dir, abs_prefix, sizeof(abs_prefix));
  termux_make_absolute_path(output_dir, abs_output, sizeof(abs_output));

  char tar_path[1024];
  char tar_cmd[2048];

  snprintf(tar_path, sizeof(tar_path), "%s/%s-%s-%s.tar.gz",
           abs_output, pkg_name, version, arch_name);

  snprintf(tar_cmd, sizeof(tar_cmd),
           "export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin && cd '%s' && tar -czf '%s' . 2>&1",
           abs_prefix, tar_path);

  char *argv[] = { "/bin/bash", "-c", tar_cmd, NULL };
  char output[4096];
  size_t output_len = 0;

  return termux_execve_capture("/bin/bash", argv, NULL, output, sizeof(output), &output_len);
}
