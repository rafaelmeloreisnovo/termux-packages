#ifndef TERMUX_PHASE_BARRIER_LOCKFREE_H
#define TERMUX_PHASE_BARRIER_LOCKFREE_H

#include <stdint.h>

#define TERMUX_MAX_THREADS 16
#define TERMUX_BARRIER_TIMEOUT_MS 30000

struct termux_phase_barrier {
  volatile uint32_t count;
  volatile uint32_t generation;
  uint32_t num_threads;
  uint8_t _pad[52];
} __attribute__((aligned(64)));

int termux_phase_barrier_init(struct termux_phase_barrier *barrier,
                              uint32_t num_threads);

int termux_phase_barrier_wait(struct termux_phase_barrier *barrier);

void termux_phase_barrier_reset(struct termux_phase_barrier *barrier);

uint32_t termux_phase_barrier_get_generation(struct termux_phase_barrier *barrier);

#endif
