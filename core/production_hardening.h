#ifndef TERMUX_PRODUCTION_HARDENING_H
#define TERMUX_PRODUCTION_HARDENING_H

#include <stdint.h>
#include <stddef.h>

/*
 * Phase 9.15: Production Hardening
 *
 * Capacidades LATENTES ativadas:
 * - Checkpoint/Resume: pausa e retomada de builds
 * - Partial Builds: compilação de subset de packages
 * - Clean Rebuild: força reconstrução ignorando .deb
 * - Error Recovery: isolamento e retry de falhas
 * - Incremental Builds: cache de artefatos
 * - Parallel Architecture: builds ARM32/ARM64 simultâneos
 */

#define TERMUX_PRODUCTION_VERSION "9.15.0"
#define TERMUX_MAX_RETRIES 3
#define TERMUX_CHECKPOINT_DIR "_checkpoints"
#define TERMUX_CACHE_DIR "_build_cache"

typedef enum {
  BUILD_STATE_INIT = 0,
  BUILD_STATE_RUNNING = 1,
  BUILD_STATE_PAUSED = 2,
  BUILD_STATE_RESUMED = 3,
  BUILD_STATE_FAILED = 4,
  BUILD_STATE_SUCCESS = 5,
  BUILD_STATE_PARTIAL = 6
} build_state_t;

typedef enum {
  RETRY_POLICY_IMMEDIATE = 0,
  RETRY_POLICY_EXPONENTIAL = 1,
  RETRY_POLICY_ADAPTIVE = 2
} retry_policy_t;

typedef struct {
  uint32_t pkg_idx;
  uint32_t layer_idx;
  char pkg_name[64];
  build_state_t state;
  uint32_t attempt_count;
  double build_time_sec;
  uint32_t exit_code;
  uint64_t timestamp;
} build_record_t;

typedef struct {
  uint32_t total_packages;
  uint32_t completed_packages;
  uint32_t failed_packages;
  uint32_t skipped_packages;
  uint32_t current_layer;
  uint64_t elapsed_ns;
  double coherence_phi;
  build_state_t state;
  char arch[32];
  char format[32];
} checkpoint_t;

typedef struct {
  build_record_t *records;
  uint32_t record_count;
  uint32_t record_capacity;
  checkpoint_t checkpoint;
} build_history_t;

typedef struct {
  char *pkg_names;
  uint32_t *pkg_indices;
  uint32_t pkg_count;
  int clean_rebuild;
  int parallel_archs;
} partial_build_config_t;

/* Checkpoint & Resume */
int production_checkpoint_save(const checkpoint_t *cp, const char *path);
int production_checkpoint_load(checkpoint_t *cp, const char *path);
int production_checkpoint_resume(const checkpoint_t *cp, const char *build_script);

/* Partial Builds */
int production_partial_build_init(partial_build_config_t *cfg, uint32_t capacity);
int production_partial_build_add(partial_build_config_t *cfg, const char *pkg_name);
int production_partial_build_execute(partial_build_config_t *cfg, const char *build_script);
void production_partial_build_free(partial_build_config_t *cfg);

/* Error Recovery */
int production_error_recovery_init(build_history_t *hist, uint32_t capacity);
int production_error_record(build_history_t *hist, const build_record_t *rec);
int production_retry_package(build_history_t *hist, uint32_t pkg_idx,
                             retry_policy_t policy, const char *build_script);
void production_error_recovery_free(build_history_t *hist);

/* Incremental Builds */
int production_cache_init(const char *cache_dir);
int production_cache_lookup(const char *pkg_name, const char *arch,
                            char *output_path, size_t max_len);
int production_cache_store(const char *pkg_name, const char *arch,
                           const char *deb_path);
int production_cache_invalidate(const char *pkg_name);

/* Parallel Architecture Builds */
int production_parallel_arch_build(const char *pkg_list_file,
                                   const char *architectures);

/* Reports & Statistics */
int production_generate_report(const build_history_t *hist,
                               char *buffer, size_t buffer_size);
int production_analyze_failures(const build_history_t *hist);

#endif /* TERMUX_PRODUCTION_HARDENING_H */
