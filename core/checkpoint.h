#ifndef TERMUX_CHECKPOINT_H
#define TERMUX_CHECKPOINT_H

#include <stdint.h>
#include <time.h>
#include "manifest.h"

struct termux_checkpoint_file {
  uint32_t magic;
  uint32_t version;
  uint32_t last_phase_index;
  time_t timestamp;
};

struct termux_checkpoint {
  uint32_t last_completed_phase;
  uint8_t phase_completed[TERMUX_MAX_PHASES];
};

struct termux_checkpoint* termux_get_checkpoint(void);
int termux_checkpoint_save(const char *checkpoint_path,
                           struct termux_build_context *ctx,
                           uint32_t last_phase_index);
int termux_checkpoint_load(const char *checkpoint_path,
                           struct termux_build_context *ctx,
                           uint32_t *out_phase_index);
int termux_checkpoint_exists(const char *checkpoint_path);
int termux_checkpoint_delete(const char *checkpoint_path);
void termux_checkpoint_record_phase(uint32_t phase_index);
uint32_t termux_checkpoint_get_resume_phase(void);
void termux_checkpoint_reset(void);

#endif
