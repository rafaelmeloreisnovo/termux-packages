#include "unified_orchestrator.h"
#include "workload_generator.h"
#include "build_orchestrator.h"
#include "phase_barrier_lockfree.h"
#include "dep_resolver.h"
#include "manifest_v2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

typedef struct {
  unified_orchestrator_t *orchestrator;
  termux_workload_t *workload;
  uint32_t layer_id;
  uint32_t pkg_count;
  uint32_t *pkg_indices;
  double coherence_phi_sum;
  uint64_t total_build_time_ns;
} layer_context_t;

typedef struct {
  char build_script_path[512];
  char output_dir[512];
  char arch[32];
  int debug_build;
  int install_deps;
  char format[32];
} build_config_t;

static uint64_t get_time_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int assign_packages_to_layers(termux_workload_t *workload,
                                     struct termux_dep_resolver *resolver,
                                     uint32_t *layer_assignment) {
  if (!workload || !resolver || !layer_assignment) return -1;

  for (uint32_t i = 0; i < workload->pkg_count; i++) {
    uint8_t depth = resolver->graph.depths ? resolver->graph.depths[i] : 0;
    layer_assignment[i] = depth % TERMUX_TOROIDAL_LAYERS;
  }

  return 0;
}

static int build_package_subprocess(const build_config_t *config,
                                    const termux_package_info_t *pkg,
                                    uint32_t pkg_idx,
                                    double *build_time_sec) {
  if (!config || !pkg || !build_time_sec) return -1;

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    char *args[] = {
      (char *)config->build_script_path,
      "-a", (char *)config->arch,
      config->debug_build ? (char *)"-d" : (char *)"",
      config->install_deps ? (char *)"-i" : (char *)"",
      "-f", (char *)config->format,
      config->output_dir[0] ? (char *)"-o" : (char *)"",
      config->output_dir[0] ? (char *)config->output_dir : (char *)"",
      (char *)pkg->name,
      NULL
    };

    execv(config->build_script_path, args);
    perror("execv");
    exit(127);
  }

  uint64_t start = get_time_ns();
  int status = 0;
  waitpid(pid, &status, 0);
  uint64_t end = get_time_ns();

  *build_time_sec = (double)(end - start) / 1e9;

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "Package %s (idx=%u) build failed\n", pkg->name, pkg_idx);
    return -1;
  }

  return 0;
}

static int execute_layer(layer_context_t *layer_ctx,
                         const build_config_t *config) {
  if (!layer_ctx || !config) return -1;

  printf("\n[Layer %u/%u] Starting build of %u packages...\n",
         layer_ctx->layer_id, TERMUX_TOROIDAL_LAYERS, layer_ctx->pkg_count);

  layer_ctx->total_build_time_ns = 0;
  layer_ctx->coherence_phi_sum = 0.0;

  for (uint32_t i = 0; i < layer_ctx->pkg_count; i++) {
    uint32_t pkg_idx = layer_ctx->pkg_indices[i];
    if (pkg_idx >= layer_ctx->workload->pkg_count) {
      fprintf(stderr, "Package index %u out of bounds\n", pkg_idx);
      continue;
    }

    termux_package_info_t *pkg = &layer_ctx->workload->packages[pkg_idx];
    double build_time_sec = 0.0;

    printf("  [%u/%u] Building %s... ", i + 1, layer_ctx->pkg_count,
           pkg->name);
    fflush(stdout);

    int ret = build_package_subprocess(config, pkg, pkg_idx, &build_time_sec);

    if (ret == 0) {
      printf("OK (%.2f sec)\n", build_time_sec);
      layer_ctx->total_build_time_ns += (uint64_t)(build_time_sec * 1e9);

      double phi = (1.0 - 0.1) * (1.0 - build_time_sec / 300.0) * (1.0 - 0.05);
      if (phi < 0.0) phi = 0.0;
      if (phi > 1.0) phi = 1.0;
      layer_ctx->coherence_phi_sum += phi;

      if (layer_ctx->orchestrator) {
        unified_orchestrator_update_coherence(layer_ctx->orchestrator,
                                              layer_ctx->layer_id,
                                              phi);
      }
    } else {
      printf("FAILED\n");
      return -1;
    }
  }

  double avg_phi = layer_ctx->pkg_count > 0 ?
                   layer_ctx->coherence_phi_sum / layer_ctx->pkg_count : 0.0;
  double layer_time_sec = (double)layer_ctx->total_build_time_ns / 1e9;

  printf("[Layer %u] Completed: avg_φ=%.4f, total_time=%.2f sec\n",
         layer_ctx->layer_id, avg_phi, layer_time_sec);

  return 0;
}

int main(int argc, char *argv[]) {
  build_config_t config = {0};
  strncpy(config.arch, "aarch64", sizeof(config.arch) - 1);
  strncpy(config.format, "debian", sizeof(config.format) - 1);
  config.debug_build = 0;
  config.install_deps = 0;

  int opt;
  while ((opt = getopt(argc, argv, "a:dif:o:b:")) != -1) {
    switch (opt) {
      case 'a':
        strncpy(config.arch, optarg, sizeof(config.arch) - 1);
        break;
      case 'd':
        config.debug_build = 1;
        break;
      case 'i':
        config.install_deps = 1;
        break;
      case 'f':
        strncpy(config.format, optarg, sizeof(config.format) - 1);
        break;
      case 'o':
        strncpy(config.output_dir, optarg, sizeof(config.output_dir) - 1);
        break;
      case 'b':
        strncpy(config.build_script_path, optarg, sizeof(config.build_script_path) - 1);
        break;
      default:
        fprintf(stderr, "Usage: %s [options]\n", argv[0]);
        fprintf(stderr, "  -a ARCH    Architecture (default: aarch64)\n");
        fprintf(stderr, "  -d         Debug build\n");
        fprintf(stderr, "  -i         Install dependencies\n");
        fprintf(stderr, "  -f FORMAT  Package format (default: debian)\n");
        fprintf(stderr, "  -o DIR     Output directory\n");
        fprintf(stderr, "  -b SCRIPT  Build script path\n");
        return 1;
    }
  }

  if (config.build_script_path[0] == 0) {
    fprintf(stderr, "Error: Build script path required (-b)\n");
    return 1;
  }

  printf("================================================================================\n");
  printf("  TERMUX ORCHESTRATED BUILD SYSTEM (Sistema Núcleo Autoral - Phase 9.13)\n");
  printf("================================================================================\n");
  printf("Configuration:\n");
  printf("  Architecture: %s\n", config.arch);
  printf("  Format: %s\n", config.format);
  printf("  Build Script: %s\n", config.build_script_path);
  if (config.output_dir[0]) {
    printf("  Output Dir: %s\n", config.output_dir);
  }
  printf("\n");

  unified_orchestrator_t *orchestrator = NULL;
  if (unified_orchestrator_alloc(&orchestrator) != 0) {
    fprintf(stderr, "Failed to allocate orchestrator\n");
    return 1;
  }

  if (unified_orchestrator_init_parallel(orchestrator, 1) != 0) {
    fprintf(stderr, "Failed to initialize orchestrator\n");
    unified_orchestrator_free(orchestrator);
    return 1;
  }

  printf("Orchestrator initialized (device: %s)\n",
         orchestrator->device_module.profiles[0].device_name);

  termux_workload_t workload = {0};
  if (termux_workload_alloc(&workload, 50) != 0) {
    fprintf(stderr, "Failed to allocate workload (using 50 sample packages)\n");
    unified_orchestrator_free(orchestrator);
    return 1;
  }

  if (termux_workload_populate_realistic(&workload) != 0) {
    fprintf(stderr, "Failed to populate workload\n");
    termux_workload_free(&workload);
    unified_orchestrator_free(orchestrator);
    return 1;
  }

  if (termux_workload_build_graph(&workload) != 0) {
    fprintf(stderr, "Failed to build dependency graph\n");
    termux_workload_free(&workload);
    unified_orchestrator_free(orchestrator);
    return 1;
  }

  uint32_t validation = termux_workload_validate_dag(&workload);
  if (validation > 0) {
    fprintf(stderr, "Warning: %u dependency issues detected\n", validation);
  }

  printf("Workload loaded: %u packages\n", workload.pkg_count);
  termux_workload_print_stats(&workload);
  printf("\n");

  uint32_t *layer_assignment = (uint32_t *)malloc(workload.pkg_count * sizeof(uint32_t));
  if (!layer_assignment) {
    fprintf(stderr, "Failed to allocate layer assignment\n");
    termux_workload_free(&workload);
    unified_orchestrator_free(orchestrator);
    return 1;
  }

  if (assign_packages_to_layers(&workload, workload.resolver, layer_assignment) != 0) {
    fprintf(stderr, "Failed to assign packages to layers\n");
    free(layer_assignment);
    termux_workload_free(&workload);
    unified_orchestrator_free(orchestrator);
    return 1;
  }

  uint32_t layers_to_build = TERMUX_TOROIDAL_LAYERS;
  uint32_t *pkgs_per_layer = (uint32_t *)calloc(layers_to_build, sizeof(uint32_t));
  if (!pkgs_per_layer) {
    fprintf(stderr, "Failed to allocate layer sizes\n");
    free(layer_assignment);
    termux_workload_free(&workload);
    unified_orchestrator_free(orchestrator);
    return 1;
  }

  for (uint32_t i = 0; i < workload.pkg_count; i++) {
    uint32_t layer = layer_assignment[i];
    if (layer < layers_to_build) {
      pkgs_per_layer[layer]++;
    }
  }

  printf("Toroidal layer distribution:\n");
  for (uint32_t i = 0; i < layers_to_build; i++) {
    if (pkgs_per_layer[i] > 0) {
      printf("  Layer %2u: %3u packages\n", i, pkgs_per_layer[i]);
    }
  }
  printf("\n");

  uint64_t total_start = get_time_ns();
  int build_failed = 0;

  for (uint32_t layer_id = 0; layer_id < layers_to_build; layer_id++) {
    if (pkgs_per_layer[layer_id] == 0) continue;

    uint32_t *layer_pkgs = (uint32_t *)malloc(pkgs_per_layer[layer_id] * sizeof(uint32_t));
    if (!layer_pkgs) {
      fprintf(stderr, "Failed to allocate layer packages\n");
      build_failed = 1;
      break;
    }

    uint32_t layer_pkg_idx = 0;
    for (uint32_t i = 0; i < workload.pkg_count; i++) {
      if (layer_assignment[i] == layer_id) {
        layer_pkgs[layer_pkg_idx++] = i;
      }
    }

    layer_context_t layer_ctx = {
      .orchestrator = orchestrator,
      .workload = &workload,
      .layer_id = layer_id,
      .pkg_count = pkgs_per_layer[layer_id],
      .pkg_indices = layer_pkgs,
      .coherence_phi_sum = 0.0,
      .total_build_time_ns = 0
    };

    if (execute_layer(&layer_ctx, &config) != 0) {
      fprintf(stderr, "Layer %u build failed\n", layer_id);
      build_failed = 1;
      free(layer_pkgs);
      break;
    }

    free(layer_pkgs);
  }

  uint64_t total_end = get_time_ns();
  double total_time_sec = (double)(total_end - total_start) / 1e9;

  printf("\n================================================================================\n");
  printf("BUILD SUMMARY\n");
  printf("================================================================================\n");
  printf("Total build time: %.2f seconds\n", total_time_sec);
  if (build_failed) {
    printf("Status: FAILED\n");
  } else {
    printf("Status: SUCCESS\n");
  }
  printf("\nOrchestrator Report:\n");
  unified_orchestrator_report(orchestrator);
  printf("================================================================================\n\n");

  free(pkgs_per_layer);
  free(layer_assignment);
  termux_workload_free(&workload);
  unified_orchestrator_free(orchestrator);

  return build_failed ? 1 : 0;
}
