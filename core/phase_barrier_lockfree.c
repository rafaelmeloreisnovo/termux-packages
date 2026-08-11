#include "phase_barrier_lockfree.h"
#include <string.h>

#define PHASE_BARRIER_SPIN_LIMIT 10000
#define PHASE_BARRIER_CACHE_LINE 64

int termux_phase_barrier_init(struct termux_phase_barrier *barrier,
                              uint32_t num_threads) {
  if (!barrier || num_threads == 0 || num_threads > TERMUX_MAX_THREADS) {
    return -1;
  }

  memset(barrier, 0, sizeof(*barrier));
  barrier->num_threads = num_threads;
  barrier->generation = 0;
  barrier->count = 0;

  return 0;
}

static inline void memory_barrier(void) {
#ifdef __GNUC__
  __sync_synchronize();
#endif
}

static inline uint32_t atomic_load(volatile uint32_t *addr) {
#ifdef __GNUC__
  return __sync_fetch_and_add(addr, 0);
#else
  return *addr;
#endif
}

static inline uint32_t atomic_add_return(volatile uint32_t *addr, uint32_t val) {
#ifdef __GNUC__
  return __sync_add_and_fetch(addr, val);
#else
  *addr += val;
  return *addr;
#endif
}

static inline uint32_t atomic_cas(volatile uint32_t *addr, uint32_t old,
                                   uint32_t new) __attribute__((unused));
static inline uint32_t atomic_cas(volatile uint32_t *addr, uint32_t old,
                                   uint32_t new) {
#ifdef __GNUC__
  return __sync_val_compare_and_swap(addr, old, new);
#else
  if (*addr == old) {
    *addr = new;
    return old;
  }
  return *addr;
#endif
}

int termux_phase_barrier_wait(struct termux_phase_barrier *barrier) {
  if (!barrier) {
    return -1;
  }

  uint32_t local_gen = atomic_load(&barrier->generation);
  uint32_t count = atomic_add_return(&barrier->count, 1);

  if (count == barrier->num_threads) {
    barrier->count = 0;
    atomic_add_return(&barrier->generation, 1);
    memory_barrier();
    return 0;
  }

  uint32_t spin_count = 0;
  while (atomic_load(&barrier->generation) == local_gen) {
    if (spin_count < PHASE_BARRIER_SPIN_LIMIT) {
      spin_count++;
      for (volatile uint32_t i = 0; i < 10; i++) {
        __asm__("");
      }
    } else {
      return -2;
    }
  }

  memory_barrier();
  return 0;
}

void termux_phase_barrier_reset(struct termux_phase_barrier *barrier) {
  if (!barrier) {
    return;
  }

  memory_barrier();
  barrier->count = 0;
  barrier->generation = 0;
  memory_barrier();
}

uint32_t termux_phase_barrier_get_generation(struct termux_phase_barrier *barrier) {
  if (!barrier) {
    return 0;
  }
  return atomic_load(&barrier->generation);
}
