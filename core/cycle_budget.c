#include "cycle_budget.h"
#include <stdio.h>
#include <string.h>

#ifdef __aarch64__
static inline uint64_t _termux_rdcycle_aarch64(void) {
  uint64_t val;
  asm volatile("mrs %0, pmccntr_el0" : "=r" (val));
  return val;
}
#else
static inline uint64_t _termux_rdcycle_generic(void) {
  static uint64_t counter = 0;
  return counter++;
}
#endif

uint64_t termux_rdcycle(void) {
#ifdef __aarch64__
  return _termux_rdcycle_aarch64();
#else
  return _termux_rdcycle_generic();
#endif
}

int termux_cycle_profile_start(cycle_profile_t *prof, uint32_t phase) {
  if (!prof || phase >= 8) return -1;

  cycle_measurement_t *m = &prof->measurements[phase];
  m->phase_id = phase;
  m->cycles_budget = TERMUX_CYCLE_BUDGET_PER_PHASE;
  m->cycles_start = termux_rdcycle();

  return 0;
}

int termux_cycle_profile_stop(cycle_profile_t *prof, uint32_t phase) {
  if (!prof || phase >= 8) return -1;

  cycle_measurement_t *m = &prof->measurements[phase];
  m->cycles_end = termux_rdcycle();
  m->cycles_actual = (uint32_t)(m->cycles_end - m->cycles_start);
  m->exceeded = m->cycles_actual > m->cycles_budget ? 1 : 0;

  prof->total_cycles += m->cycles_actual;
  if (m->cycles_actual > prof->peak_cycles) {
    prof->peak_cycles = m->cycles_actual;
  }
  if (m->exceeded) {
    prof->violations++;
  }

  return 0;
}

int termux_cycle_profile_validate(const cycle_profile_t *prof) {
  if (!prof) return -1;

  if (prof->total_cycles > TERMUX_MAX_CYCLES_PER_PACKAGE) return -2;
  if (prof->violations > 0) return -3;

  return 0;
}

double termux_cycle_efficiency_compute(const cycle_profile_t *prof) {
  if (!prof || prof->peak_cycles == 0) return 0.0;

  double budget = TERMUX_MAX_CYCLES_PER_PACKAGE;
  double actual = (double)prof->total_cycles;
  double efficiency = 1.0 - (actual / budget);

  return efficiency > 0.0 ? efficiency : 0.0;
}

void termux_cycle_profile_print(const cycle_profile_t *prof) {
  if (!prof) return;

  printf("\n=== Cycle Profile Report ===\n");
  printf("Total cycles: %u / %u budget\n",
         prof->total_cycles, TERMUX_MAX_CYCLES_PER_PACKAGE);
  printf("Peak cycles: %u / %u (per-phase budget)\n",
         prof->peak_cycles, TERMUX_CYCLE_BUDGET_PER_PHASE);
  printf("Violations: %u / 8 phases\n", prof->violations);
  printf("Efficiency: %.2f%%\n\n", prof->efficiency * 100.0);

  printf("Per-phase breakdown:\n");
  for (int i = 0; i < 8; i++) {
    const cycle_measurement_t *m = &prof->measurements[i];
    printf("  Phase %d: %u cycles %s\n",
           i, m->cycles_actual,
           m->exceeded ? "(EXCEEDED)" : "(ok)");
  }
}
