#define _GNU_SOURCE
#include "cpu_affinity.h"
#include <string.h>

#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#endif

int termux_cpu_affinity_set(uint32_t cpu_id) {
#ifdef __linux__
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_id, &cpuset);

  pthread_t thread = pthread_self();
  int ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  return (ret == 0) ? 0 : -1;
#else
  (void)cpu_id;
  return -1;
#endif
}

int termux_cpu_affinity_get(uint32_t *cpu_id) {
#ifdef __linux__
  if (!cpu_id) {
    return -1;
  }

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  pthread_t thread = pthread_self();
  int ret = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (ret != 0) {
    return -1;
  }

  for (uint32_t i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, &cpuset)) {
      *cpu_id = i;
      return 0;
    }
  }
  return -1;
#else
  (void)cpu_id;
  return -1;
#endif
}

uint32_t termux_cpu_count(void) {
  uint32_t count = 1;
#ifdef __linux__
  long result = sysconf(_SC_NPROCESSORS_ONLN);
  if (result > 0) {
    count = (uint32_t)result;
  }
#endif
  return count;
}

int termux_numa_node_get(uint32_t *node_id) {
#ifdef __linux__
  if (!node_id) {
    return -1;
  }

  uint32_t cpu_id;
  int ret = termux_cpu_affinity_get(&cpu_id);
  if (ret != 0) {
    return -1;
  }

  *node_id = cpu_id / 4;
  return 0;
#else
  (void)node_id;
  return -1;
#endif
}

int termux_cpu_affinity_mask_set(const struct termux_cpu_mask *mask) {
#ifdef __linux__
  if (!mask) {
    return -1;
  }

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  for (uint32_t i = 0; i < mask->cpu_count; i++) {
    CPU_SET(mask->cpu_ids[i], &cpuset);
  }

  pthread_t thread = pthread_self();
  int ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  return (ret == 0) ? 0 : -1;
#else
  (void)mask;
  return -1;
#endif
}

int termux_cpu_affinity_mask_get(struct termux_cpu_mask *mask) {
#ifdef __linux__
  if (!mask) {
    return -1;
  }

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  pthread_t thread = pthread_self();
  int ret = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (ret != 0) {
    return -1;
  }

  mask->cpu_count = 0;
  for (uint32_t i = 0; i < CPU_SETSIZE && mask->cpu_count < TERMUX_MAX_CPUS; i++) {
    if (CPU_ISSET(i, &cpuset)) {
      mask->cpu_ids[mask->cpu_count++] = i;
    }
  }
  return 0;
#else
  (void)mask;
  return -1;
#endif
}
