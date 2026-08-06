#ifndef TERMUX_BUILD_EXEC_H
#define TERMUX_BUILD_EXEC_H

#include <stdint.h>
#include "manifest.h"

int termux_exec_configure(struct termux_build_context *ctx,
                          const char *source_dir,
                          const char *build_dir);

int termux_exec_make(struct termux_build_context *ctx,
                     const char *build_dir,
                     uint32_t num_jobs);

int termux_exec_make_install(struct termux_build_context *ctx,
                             const char *build_dir,
                             const char *prefix_dir);

int termux_collect_artifacts(const char *prefix_dir,
                             const char *output_dir,
                             const char *pkg_name,
                             const char *version,
                             const char *arch_name);

const char *termux_find_bash(void);

#endif
