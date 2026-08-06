#include "checkpoint.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define CHECKPOINT_MAGIC 0x43504B54

static struct termux_checkpoint global_checkpoint;

struct termux_checkpoint* termux_get_checkpoint(void) {
  return &global_checkpoint;
}

int termux_checkpoint_save(const char *checkpoint_path,
                           struct termux_build_context *ctx,
                           uint32_t last_phase_index) {
  if (!checkpoint_path || !ctx) return -1;

  int fd = open(checkpoint_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return -1;
  }

  struct termux_checkpoint_file hdr = {
    .magic = CHECKPOINT_MAGIC,
    .version = 1,
    .last_phase_index = last_phase_index,
    .timestamp = time(NULL),
  };

  if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    close(fd);
    return -1;
  }

  if (write(fd, &ctx->pkg, sizeof(ctx->pkg)) != sizeof(ctx->pkg)) {
    close(fd);
    return -1;
  }

  if (write(fd, &ctx->output_pos, sizeof(uint32_t)) != sizeof(uint32_t)) {
    close(fd);
    return -1;
  }

  if (write(fd, ctx->build_output, ctx->output_pos) != (ssize_t)ctx->output_pos) {
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}

int termux_checkpoint_load(const char *checkpoint_path,
                           struct termux_build_context *ctx,
                           uint32_t *out_phase_index) {
  if (!checkpoint_path || !ctx || !out_phase_index) return -1;

  if (access(checkpoint_path, F_OK) != 0) {
    return -1;
  }

  int fd = open(checkpoint_path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  struct termux_checkpoint_file hdr;
  if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    close(fd);
    return -1;
  }

  if (hdr.magic != CHECKPOINT_MAGIC || hdr.version != 1) {
    close(fd);
    return -1;
  }

  if (read(fd, &ctx->pkg, sizeof(ctx->pkg)) != sizeof(ctx->pkg)) {
    close(fd);
    return -1;
  }

  uint32_t output_pos = 0;
  if (read(fd, &output_pos, sizeof(uint32_t)) != sizeof(uint32_t)) {
    close(fd);
    return -1;
  }

  if (output_pos > ctx->output_size) {
    close(fd);
    return -1;
  }

  if (read(fd, ctx->build_output, output_pos) != (ssize_t)output_pos) {
    close(fd);
    return -1;
  }

  ctx->output_pos = output_pos;
  *out_phase_index = hdr.last_phase_index;

  close(fd);
  return 0;
}

int termux_checkpoint_exists(const char *checkpoint_path) {
  if (!checkpoint_path) return 0;
  return access(checkpoint_path, F_OK) == 0 ? 1 : 0;
}

int termux_checkpoint_delete(const char *checkpoint_path) {
  if (!checkpoint_path) return -1;
  return unlink(checkpoint_path);
}

void termux_checkpoint_record_phase(uint32_t phase_index) {
  global_checkpoint.last_completed_phase = phase_index;
  global_checkpoint.phase_completed[phase_index] = 1;
}

uint32_t termux_checkpoint_get_resume_phase(void) {
  for (uint32_t i = 0; i < TERMUX_MAX_PHASES; i++) {
    if (!global_checkpoint.phase_completed[i]) {
      return i;
    }
  }
  return TERMUX_MAX_PHASES;
}

void termux_checkpoint_reset(void) {
  memset(&global_checkpoint, 0, sizeof(global_checkpoint));
}
