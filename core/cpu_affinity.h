#ifndef TERMUX_CPU_AFFINITY_H
#define TERMUX_CPU_AFFINITY_H

#include <stdint.h>

#define TERMUX_MAX_CPUS 16

struct termux_cpu_mask {
  uint32_t cpu_ids[TERMUX_MAX_CPUS];
  uint32_t cpu_count;
};

int termux_cpu_affinity_set(uint32_t cpu_id);

int termux_cpu_affinity_get(uint32_t *cpu_id);

uint32_t termux_cpu_count(void);

int termux_numa_node_get(uint32_t *node_id);

int termux_cpu_affinity_mask_set(const struct termux_cpu_mask *mask);

int termux_cpu_affinity_mask_get(struct termux_cpu_mask *mask);

#endif
