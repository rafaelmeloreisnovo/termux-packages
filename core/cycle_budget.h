#ifndef TERMUX_CYCLE_BUDGET_H
#define TERMUX_CYCLE_BUDGET_H

#include <stdint.h>
#include <stddef.h>

#define TERMUX_CYCLE_BUDGET_PER_PHASE 42
#define TERMUX_MAX_CYCLES_PER_PACKAGE (TERMUX_CYCLE_BUDGET_PER_PHASE * 8)

typedef struct {
  uint32_t phase_id;
  uint64_t cycles_start;
  uint64_t cycles_end;
  uint32_t cycles_actual;
  uint32_t cycles_budget;
  uint8_t exceeded;
} cycle_measurement_t;

typedef struct {
  cycle_measurement_t measurements[8];
  uint32_t total_cycles;
  uint32_t peak_cycles;
  uint32_t violations;
  double efficiency;
} cycle_profile_t;

uint64_t termux_rdcycle(void);

int termux_cycle_profile_start(cycle_profile_t *prof, uint32_t phase);

int termux_cycle_profile_stop(cycle_profile_t *prof, uint32_t phase);

int termux_cycle_profile_validate(const cycle_profile_t *prof);

double termux_cycle_efficiency_compute(const cycle_profile_t *prof);

void termux_cycle_profile_print(const cycle_profile_t *prof);

#endif
