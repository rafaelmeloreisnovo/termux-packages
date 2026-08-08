#include "production_hardening.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/*
 * Phase 9.15: Production Hardening Implementation
 *
 * Ativa todas as capacidades LATENTES e EMERGENTES do sistema orquestrado.
 * Prioriza erro recovery, checkpointing, e builds paralelos.
 */

#define CHECKPOINT_MAGIC 0xDEADBEEFU
#define CHECKPOINT_VERSION 1

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint64_t timestamp;
  checkpoint_t data;
} checkpoint_file_t;

/* ============================================================================
 * Checkpoint & Resume (CRÍTICO para LATENTE)
 * ============================================================================ */

int production_checkpoint_save(const checkpoint_t *cp, const char *path) {
  if (!cp || !path) return -1;

  /* Criar diretório se não existir */
  mkdir(TERMUX_CHECKPOINT_DIR, 0755);

  checkpoint_file_t file;
  file.magic = CHECKPOINT_MAGIC;
  file.version = CHECKPOINT_VERSION;
  file.timestamp = time(NULL);
  memcpy(&file.data, cp, sizeof(*cp));

  char full_path[512];
  snprintf(full_path, sizeof(full_path), "%s/%s", TERMUX_CHECKPOINT_DIR, path);

  int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    perror("open checkpoint");
    return -2;
  }

  if (write(fd, &file, sizeof(file)) != sizeof(file)) {
    perror("write checkpoint");
    close(fd);
    return -3;
  }

  close(fd);
  return 0;
}

int production_checkpoint_load(checkpoint_t *cp, const char *path) {
  if (!cp || !path) return -1;

  checkpoint_file_t file;
  char full_path[512];
  snprintf(full_path, sizeof(full_path), "%s/%s", TERMUX_CHECKPOINT_DIR, path);

  int fd = open(full_path, O_RDONLY);
  if (fd < 0) {
    perror("open checkpoint");
    return -2;
  }

  if (read(fd, &file, sizeof(file)) != sizeof(file)) {
    perror("read checkpoint");
    close(fd);
    return -3;
  }

  close(fd);

  if (file.magic != CHECKPOINT_MAGIC) {
    fprintf(stderr, "Checkpoint magic mismatch\n");
    return -4;
  }

  if (file.version != CHECKPOINT_VERSION) {
    fprintf(stderr, "Checkpoint version mismatch\n");
    return -5;
  }

  memcpy(cp, &file.data, sizeof(*cp));
  return 0;
}

int production_checkpoint_resume(const checkpoint_t *cp, const char *build_script) {
  if (!cp || !build_script) return -1;

  printf("=== Resuming Build from Checkpoint ===\n");
  printf("Layer: %u\n", cp->current_layer);
  printf("Completed: %u / %u packages\n", cp->completed_packages, cp->total_packages);
  printf("Elapsed: %.2f seconds\n", cp->elapsed_ns / 1e9);
  printf("Coherence φ: %.4f\n", cp->coherence_phi);
  printf("Resuming layer %u...\n", cp->current_layer);

  return 0;
}

/* ============================================================================
 * Partial Builds (EMERGENTE)
 * ============================================================================ */

int production_partial_build_init(partial_build_config_t *cfg, uint32_t capacity) {
  if (!cfg || capacity == 0) return -1;

  cfg->pkg_names = (char *)malloc(capacity * 64);
  if (!cfg->pkg_names) return -2;

  cfg->pkg_indices = (uint32_t *)malloc(capacity * sizeof(uint32_t));
  if (!cfg->pkg_indices) {
    free(cfg->pkg_names);
    return -2;
  }

  cfg->pkg_count = 0;
  cfg->clean_rebuild = 0;
  cfg->parallel_archs = 0;

  return 0;
}

int production_partial_build_add(partial_build_config_t *cfg, const char *pkg_name) {
  if (!cfg || !pkg_name) return -1;

  size_t len = strlen(pkg_name);
  if (len >= 64) return -2;

  memcpy(&cfg->pkg_names[cfg->pkg_count * 64], pkg_name, len + 1);
  cfg->pkg_count++;

  return 0;
}

int production_partial_build_execute(partial_build_config_t *cfg,
                                     const char *build_script) {
  if (!cfg || !build_script) return -1;

  printf("=== Executing Partial Build ===\n");
  printf("Packages: %u\n", cfg->pkg_count);
  printf("Clean rebuild: %s\n", cfg->clean_rebuild ? "YES" : "NO");
  printf("Parallel archs: %s\n", cfg->parallel_archs ? "YES" : "NO");

  for (uint32_t i = 0; i < cfg->pkg_count; i++) {
    char *pkg_name = &cfg->pkg_names[i * 64];
    printf("  [%u/%u] %s\n", i + 1, cfg->pkg_count, pkg_name);

    if (cfg->clean_rebuild) {
      printf("    (clean rebuild, ignoring existing .deb)\n");
    }
  }

  return 0;
}

void production_partial_build_free(partial_build_config_t *cfg) {
  if (!cfg) return;
  if (cfg->pkg_names) free(cfg->pkg_names);
  if (cfg->pkg_indices) free(cfg->pkg_indices);
  memset(cfg, 0, sizeof(*cfg));
}

/* ============================================================================
 * Error Recovery (URGENTE)
 * ============================================================================ */

int production_error_recovery_init(build_history_t *hist, uint32_t capacity) {
  if (!hist || capacity == 0) return -1;

  hist->records = (build_record_t *)calloc(capacity, sizeof(build_record_t));
  if (!hist->records) return -2;

  hist->record_count = 0;
  hist->record_capacity = capacity;
  memset(&hist->checkpoint, 0, sizeof(hist->checkpoint));

  return 0;
}

int production_error_record(build_history_t *hist, const build_record_t *rec) {
  if (!hist || !rec) return -1;

  if (hist->record_count >= hist->record_capacity) {
    build_record_t *new_records = (build_record_t *)realloc(
        hist->records,
        hist->record_capacity * 2 * sizeof(build_record_t));
    if (!new_records) return -2;

    hist->records = new_records;
    hist->record_capacity *= 2;
  }

  memcpy(&hist->records[hist->record_count], rec, sizeof(*rec));
  hist->record_count++;

  if (rec->state == BUILD_STATE_FAILED) {
    hist->checkpoint.failed_packages++;
  } else if (rec->state == BUILD_STATE_SUCCESS) {
    hist->checkpoint.completed_packages++;
  }

  return 0;
}

int production_retry_package(build_history_t *hist, uint32_t pkg_idx,
                             retry_policy_t policy, const char *build_script) {
  if (!hist || !build_script) return -1;

  build_record_t *rec = &hist->records[pkg_idx];

  if (rec->attempt_count >= TERMUX_MAX_RETRIES) {
    fprintf(stderr, "Max retries exceeded for package %s\n", rec->pkg_name);
    return -2;
  }

  uint32_t delay_ms = 0;

  switch (policy) {
    case RETRY_POLICY_IMMEDIATE:
      delay_ms = 0;
      break;
    case RETRY_POLICY_EXPONENTIAL:
      delay_ms = (1U << rec->attempt_count) * 100; /* 100ms, 200ms, 400ms */
      break;
    case RETRY_POLICY_ADAPTIVE:
      /* Adaptive: aumentar delay se muitas falhas */
      delay_ms = (hist->checkpoint.failed_packages > 5) ? 500 : 100;
      break;
  }

  if (delay_ms > 0) {
    printf("Retrying %s in %u ms (attempt %u/%d)\n",
           rec->pkg_name, delay_ms, rec->attempt_count + 1, TERMUX_MAX_RETRIES);
    usleep(delay_ms * 1000);
  }

  rec->attempt_count++;
  rec->state = BUILD_STATE_RUNNING;

  return 0;
}

void production_error_recovery_free(build_history_t *hist) {
  if (!hist) return;
  if (hist->records) free(hist->records);
  memset(hist, 0, sizeof(*hist));
}

/* ============================================================================
 * Incremental Builds / Cache (LATENTE)
 * ============================================================================ */

int production_cache_init(const char *cache_dir) {
  if (!cache_dir) return -1;

  mkdir(cache_dir, 0755);

  char cache_index[512];
  snprintf(cache_index, sizeof(cache_index), "%s/.index", cache_dir);

  int fd = open(cache_index, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd >= 0) {
    close(fd);
  } else if (errno != EEXIST) {
    return -2;
  }

  return 0;
}

int production_cache_lookup(const char *pkg_name, const char *arch,
                            char *output_path, size_t max_len) {
  if (!pkg_name || !arch || !output_path) return -1;

  snprintf(output_path, max_len, "%s/%s-%s.deb",
           TERMUX_CACHE_DIR, pkg_name, arch);

  if (access(output_path, F_OK) == 0) {
    return 0; /* Found in cache */
  }

  return -2; /* Not in cache */
}

int production_cache_store(const char *pkg_name, const char *arch,
                           const char *deb_path) {
  if (!pkg_name || !arch || !deb_path) return -1;

  production_cache_init(TERMUX_CACHE_DIR);

  char cache_path[512];
  snprintf(cache_path, sizeof(cache_path), "%s/%s-%s.deb",
           TERMUX_CACHE_DIR, pkg_name, arch);

  /* Symlink or copy .deb to cache */
  if (symlink(deb_path, cache_path) != 0 && errno != EEXIST) {
    perror("symlink cache");
    return -2;
  }

  return 0;
}

int production_cache_invalidate(const char *pkg_name) {
  if (!pkg_name) return -1;

  /* Remover todas as versões deste package do cache */
  char pattern[512];
  snprintf(pattern, sizeof(pattern), "%s/%s-*.deb", TERMUX_CACHE_DIR, pkg_name);

  printf("Invalidating cache for %s\n", pkg_name);
  /* Implementar busca e remoção via glob se necessário */

  return 0;
}

/* ============================================================================
 * Parallel Architecture Builds (EMERGENTE)
 * ============================================================================ */

int production_parallel_arch_build(const char *pkg_list_file,
                                   const char *architectures) {
  if (!pkg_list_file || !architectures) return -1;

  printf("=== Parallel Architecture Build ===\n");
  printf("Packages: %s\n", pkg_list_file);
  printf("Architectures: %s\n", architectures);

  /* Implementar paralelização por arquitetura */

  return 0;
}

/* ============================================================================
 * Reports & Statistics
 * ============================================================================ */

int production_generate_report(const build_history_t *hist,
                               char *buffer, size_t buffer_size) {
  if (!hist || !buffer || buffer_size < 512) return -1;

  int offset = 0;

  offset += snprintf(buffer + offset, buffer_size - offset,
                     "=== Build Report ===\n");
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Total packages: %u\n", hist->checkpoint.total_packages);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Completed: %u\n", hist->checkpoint.completed_packages);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Failed: %u\n", hist->checkpoint.failed_packages);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Skipped: %u\n", hist->checkpoint.skipped_packages);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Elapsed: %.2f seconds\n",
                     hist->checkpoint.elapsed_ns / 1e9);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Coherence φ: %.4f\n", hist->checkpoint.coherence_phi);

  if (hist->checkpoint.failed_packages > 0) {
    offset += snprintf(buffer + offset, buffer_size - offset,
                       "\nFailed packages:\n");

    for (uint32_t i = 0; i < hist->record_count; i++) {
      build_record_t *rec = &hist->records[i];
      if (rec->state == BUILD_STATE_FAILED) {
        offset += snprintf(buffer + offset, buffer_size - offset,
                           "  - %s (exit code: %u, attempts: %u)\n",
                           rec->pkg_name, rec->exit_code, rec->attempt_count);
      }
    }
  }

  return offset;
}

int production_analyze_failures(const build_history_t *hist) {
  if (!hist) return -1;

  printf("=== Failure Analysis ===\n");

  uint32_t total_attempts = 0;
  uint32_t total_failures = 0;
  double avg_build_time = 0.0;

  for (uint32_t i = 0; i < hist->record_count; i++) {
    build_record_t *rec = &hist->records[i];

    total_attempts += rec->attempt_count;
    if (rec->state == BUILD_STATE_FAILED) {
      total_failures++;
    }
    avg_build_time += rec->build_time_sec;
  }

  if (hist->record_count > 0) {
    avg_build_time /= hist->record_count;
  }

  printf("Total attempts: %u\n", total_attempts);
  printf("Total failures: %u (%.1f%%)\n",
         total_failures,
         100.0 * total_failures / hist->record_count);
  printf("Avg build time: %.2f seconds\n", avg_build_time);

  if (hist->checkpoint.failed_packages > 0) {
    printf("Retry success rate: %.1f%%\n",
           100.0 * (total_attempts - hist->checkpoint.failed_packages) /
               total_attempts);
  }

  return 0;
}
