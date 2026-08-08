#ifndef TERMUX_SOURCE_DOWNLOAD_H
#define TERMUX_SOURCE_DOWNLOAD_H

#include "manifest.h"

/*
 * Materialize ctx->source_dir from the manifest URL with SHA-256 verification,
 * cache validation, atomic cache promotion and extraction.
 *
 * This is the acquisition primitive. The build state-machine wrapper decides
 * whether an explicitly pre-materialized source tree can be reused first.
 */
int termux_acquire_source(struct termux_build_context *ctx);

#endif
