// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Rafael M. R. — rafaelmeloreisnovo
#include "rmr_vector_field.h"

/*
 * VECTRA/Rafaelia deterministic vector-field kernel.
 * Hot path contract: fixed Q16.16 integer math, no heap, no libc calls, no libm,
 * bounded loops only.  The Attractor #22 VOID paradox is flagged, not patched.
 */

#define RMR_VECTOR_SEED 0x52414656u
#define RMR_VECTOR_GOLD 0x9E3779B9u
#define RMR_VECTOR_CRC_MUL 0x01000193u
#define RMR_VECTOR_NODE_MOD 1000u

static const u32 k_rmr_vector_numbers[10] = {
  56u, 70u, 14u, 7u, 50u, 35u, 777u, 555u, 936u, 999u
};

static u32 rmr_vf_mask_nonzero(u32 x) {
  return 0u - ((x | (0u - x)) >> 31);
}

static u32 rmr_vf_mask_eq(u32 a, u32 b) {
  return ~rmr_vf_mask_nonzero(a ^ b);
}

static u32 rmr_vf_select(u32 mask, u32 a, u32 b) {
  return (a & mask) | (b & ~mask);
}

static u32 rmr_vf_q16_mul(u32 a, u32 b) {
  return (u32)(((u64)a * (u64)b) >> 16);
}

static u32 rmr_vf_mix(u32 h, u32 x) {
  h ^= x;
  h *= RMR_VECTOR_CRC_MUL;
  h ^= h >> 13;
  h += RMR_VECTOR_GOLD;
  return h;
}

static void rmr_vf_state_copy(RmR_VectorFieldState *dst, const RmR_VectorFieldState *src) {
  dst->n_raw = src->n_raw;
  dst->n_mod42 = src->n_mod42;
  dst->arc_deg = src->arc_deg;
  dst->chord_q16 = src->chord_q16;
  dst->h_q16 = src->h_q16;
  dst->toroid_node = src->toroid_node;
  dst->spiral_q16 = src->spiral_q16;
  dst->gap_q16 = src->gap_q16;
  dst->audit_crc = src->audit_crc;
  dst->phi_q8 = src->phi_q8;
  dst->flags = src->flags;
  dst->watchdog = src->watchdog;
}

static u32 rmr_vf_chord_q16(u32 deg) {
  u32 d = deg % RMR_VECTOR_ARC_BASE;
  u32 over = 0u - (u32)(d > 180u);
  u32 folded = rmr_vf_select(over, RMR_VECTOR_ARC_BASE - d, d);
  /* folded <= 180, therefore folded*65536 fits u32 and avoids ARM32 u64 div. */
  return (folded * RMR_VECTOR_Q16_ONE) / 180u;
}

/* Equivalent to the previous 64-bit sum modulo 1000, but each term is reduced
 * first. This preserves Z/1000Z geometry while avoiding __aeabi_uldivmod on
 * freestanding ARM32. */
static u32 rmr_vf_toroid_node(const RmR_VectorFieldState *s) {
  u32 acc = ((s->n_raw % RMR_VECTOR_NODE_MOD) * 17u) % RMR_VECTOR_NODE_MOD;
  acc += ((s->n_mod42 % RMR_VECTOR_NODE_MOD) * RMR_VECTOR_MOD_BASE) % RMR_VECTOR_NODE_MOD;
  acc += ((s->arc_deg % RMR_VECTOR_NODE_MOD) * 3u) % RMR_VECTOR_NODE_MOD;
  acc += (s->h_q16 >> 6) % RMR_VECTOR_NODE_MOD;
  return acc % RMR_VECTOR_NODE_MOD;
}

static u32 rmr_vf_load_number(u32 index) {
  u32 raw = index;
  for (u32 i = 0u; i < 10u; ++i) {
    raw = rmr_vf_select(rmr_vf_mask_eq(index, i), k_rmr_vector_numbers[i], raw);
  }
  return raw;
}

static void rmr_vf_step_contract(RmR_VectorFieldState *s) {
  s->gap_q16 = rmr_vf_q16_mul(s->gap_q16, RMR_VECTOR_SQRT3_OVER_2_Q16);
  s->spiral_q16 = rmr_vf_q16_mul(s->spiral_q16, RMR_VECTOR_SQRT3_OVER_2_Q16);
  s->audit_crc = rmr_vf_mix(s->audit_crc, s->gap_q16 ^ s->spiral_q16);
}

void RmR_VectorField_Init(RmR_VectorFieldState *state) {
  if (!state) return;
  state->n_raw = 0u;
  state->n_mod42 = 0u;
  state->arc_deg = 0u;
  state->chord_q16 = 0u;
  state->h_q16 = 0u;
  state->toroid_node = 0u;
  state->spiral_q16 = RMR_VECTOR_Q16_ONE;
  state->gap_q16 = RMR_VECTOR_Q16_ONE;
  state->audit_crc = RMR_VECTOR_SEED;
  state->phi_q8 = RMR_VECTOR_PHI_Q8_INIT;
  state->flags = RMR_VECTOR_FLAG_OK;
  state->watchdog = 0u;
}

u32 RmR_VectorField_RunIndex(RmR_VectorFieldState *state, u32 index, u32 correction_steps) {
  if (!state) return RMR_VECTOR_FLAG_FAILSAFE;

  RmR_VectorFieldState rollback;
  rmr_vf_state_copy(&rollback, state);
  u32 capped = correction_steps;
  u32 wd_mask = 0u - (u32)(correction_steps > RMR_VECTOR_WATCHDOG_MAX);
  capped = rmr_vf_select(wd_mask, RMR_VECTOR_WATCHDOG_MAX, capped);

  state->n_raw = rmr_vf_load_number(index);
  state->n_mod42 = state->n_raw % RMR_VECTOR_MOD_BASE;
  state->arc_deg = state->n_raw % RMR_VECTOR_ARC_BASE;
  state->chord_q16 = rmr_vf_chord_q16(state->arc_deg);
  state->h_q16 = rmr_vf_q16_mul(state->chord_q16, RMR_VECTOR_SQRT3_OVER_2_Q16);
  state->toroid_node = rmr_vf_toroid_node(state);

  for (u32 i = 0u; i < capped; ++i) {
    rmr_vf_step_contract(state);
  }

  state->phi_q8 += capped;
  state->phi_q8 = rmr_vf_select(0u - (u32)(state->phi_q8 > RMR_VECTOR_PHI_Q8_MAX),
                                RMR_VECTOR_PHI_Q8_MAX,
                                state->phi_q8);
  state->watchdog = capped;
  state->flags |= wd_mask & RMR_VECTOR_FLAG_WATCHDOG;
  state->flags |= rmr_vf_mask_eq(state->n_mod42, 22u) & RMR_VECTOR_FLAG_VOID22;
  state->audit_crc = rmr_vf_mix(state->audit_crc, state->n_raw);
  state->audit_crc = rmr_vf_mix(state->audit_crc, state->n_mod42);
  state->audit_crc = rmr_vf_mix(state->audit_crc, state->arc_deg);
  state->audit_crc = rmr_vf_mix(state->audit_crc, state->chord_q16);
  state->audit_crc = rmr_vf_mix(state->audit_crc, state->h_q16);
  state->audit_crc = rmr_vf_mix(state->audit_crc, state->toroid_node);
  state->audit_crc = rmr_vf_mix(state->audit_crc, state->phi_q8 ^ state->flags);

  if (state->gap_q16 == 0u || state->spiral_q16 == 0u) {
    rmr_vf_state_copy(state, &rollback);
    state->flags |= RMR_VECTOR_FLAG_ROLLBACK | RMR_VECTOR_FLAG_FAILSAFE;
  }

  return state->flags;
}

u32 RmR_VectorField_RunBytecode(RmR_VectorFieldState *state, const u8 *bytecode, u32 len) {
  if (!state || !bytecode) return RMR_VECTOR_FLAG_FAILSAFE;

  RmR_VectorFieldState rollback;
  rmr_vf_state_copy(&rollback, state);
  u32 pc = 0u;
  u32 running = 1u;

  while (running && pc + 1u < len) {
    const u32 op = bytecode[pc];
    const u32 arg = bytecode[pc + 1u];
    u32 known = 1u;

    if (op == RMR_VECTOR_OP_LOAD_NUM) {
      state->n_raw = rmr_vf_load_number(arg);
      state->audit_crc = rmr_vf_mix(state->audit_crc, state->n_raw);
    } else if (op == RMR_VECTOR_OP_MOD42) {
      state->n_mod42 = state->n_raw % RMR_VECTOR_MOD_BASE;
      state->flags |= rmr_vf_mask_eq(state->n_mod42, 22u) & RMR_VECTOR_FLAG_VOID22;
      state->audit_crc = rmr_vf_mix(state->audit_crc, state->n_mod42);
    } else if (op == RMR_VECTOR_OP_ARC360) {
      state->arc_deg = state->n_raw % RMR_VECTOR_ARC_BASE;
      state->audit_crc = rmr_vf_mix(state->audit_crc, state->arc_deg);
    } else if (op == RMR_VECTOR_OP_CHORD_Q16) {
      state->chord_q16 = rmr_vf_chord_q16(state->arc_deg);
      state->audit_crc = rmr_vf_mix(state->audit_crc, state->chord_q16);
    } else if (op == RMR_VECTOR_OP_H_EQ_Q16) {
      state->h_q16 = rmr_vf_q16_mul(state->chord_q16, RMR_VECTOR_SQRT3_OVER_2_Q16);
      state->audit_crc = rmr_vf_mix(state->audit_crc, state->h_q16);
    } else if (op == RMR_VECTOR_OP_TOROID_NODE) {
      state->toroid_node = rmr_vf_toroid_node(state);
      state->audit_crc = rmr_vf_mix(state->audit_crc, state->toroid_node);
    } else if (op == RMR_VECTOR_OP_CORRECT) {
      const u32 remaining = (state->watchdog < RMR_VECTOR_WATCHDOG_MAX)
                              ? (RMR_VECTOR_WATCHDOG_MAX - state->watchdog)
                              : 0u;
      const u32 overflow = (u32)(arg > remaining);
      const u32 capped = rmr_vf_select(0u - overflow, remaining, arg);

      for (u32 i = 0u; i < capped; ++i) {
        rmr_vf_step_contract(state);
      }
      state->watchdog += capped;

      if (overflow || state->gap_q16 == 0u || state->spiral_q16 == 0u) {
        rmr_vf_state_copy(state, &rollback);
        state->flags |= RMR_VECTOR_FLAG_ROLLBACK |
                        RMR_VECTOR_FLAG_WATCHDOG |
                        RMR_VECTOR_FLAG_FAILSAFE;
        return state->flags;
      }
    } else if (op == RMR_VECTOR_OP_AUDIT) {
      const u32 ok = rmr_vf_mask_nonzero(state->n_raw) &
                     (rmr_vf_mask_nonzero(state->chord_q16) | rmr_vf_mask_eq(state->arc_deg, 0u)) &
                     (rmr_vf_mask_nonzero(state->h_q16) | rmr_vf_mask_eq(state->chord_q16, 0u));
      const u32 dec = rmr_vf_select(0u - (u32)(state->phi_q8 > 10u), 10u, 0u);
      state->phi_q8 = rmr_vf_select(ok, state->phi_q8 + 5u, state->phi_q8 - dec);
      state->phi_q8 = rmr_vf_select(0u - (u32)(state->phi_q8 > RMR_VECTOR_PHI_Q8_MAX),
                                    RMR_VECTOR_PHI_Q8_MAX,
                                    state->phi_q8);
      state->audit_crc = rmr_vf_mix(state->audit_crc, state->phi_q8);
    } else if (op == RMR_VECTOR_OP_SEAL) {
      running = 0u;
    } else {
      known = 0u;
    }

    if (!known) {
      rmr_vf_state_copy(state, &rollback);
      state->flags |= RMR_VECTOR_FLAG_ROLLBACK | RMR_VECTOR_FLAG_FAILSAFE;
      return state->flags;
    }
    pc += 2u;
  }

  if (running && pc != len) {
    rmr_vf_state_copy(state, &rollback);
    state->flags |= RMR_VECTOR_FLAG_ROLLBACK | RMR_VECTOR_FLAG_FAILSAFE;
  }
  return state->flags;
}

u32 RmR_VectorField_Checksum(const RmR_VectorFieldState *state) {
  if (!state) return 0u;
  u32 h = RMR_VECTOR_SEED;
  h = rmr_vf_mix(h, state->n_raw);
  h = rmr_vf_mix(h, state->n_mod42);
  h = rmr_vf_mix(h, state->arc_deg);
  h = rmr_vf_mix(h, state->chord_q16);
  h = rmr_vf_mix(h, state->h_q16);
  h = rmr_vf_mix(h, state->toroid_node);
  h = rmr_vf_mix(h, state->spiral_q16);
  h = rmr_vf_mix(h, state->gap_q16);
  h = rmr_vf_mix(h, state->audit_crc);
  h = rmr_vf_mix(h, state->phi_q8);
  h = rmr_vf_mix(h, state->flags);
  h = rmr_vf_mix(h, state->watchdog);
  return h;
}

u32 RmR_VectorField_SmokeSignature(void) {
  RmR_VectorFieldState s;
  RmR_VectorField_Init(&s);
  for (u32 i = 0u; i < 10u; ++i) {
    (void)RmR_VectorField_RunIndex(&s, i, 7u);
  }
  return RmR_VectorField_Checksum(&s);
}
