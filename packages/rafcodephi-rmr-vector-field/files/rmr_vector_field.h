// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) Rafael M. R. — rafaelmeloreisnovo
#ifndef RMR_VECTOR_FIELD_H
#define RMR_VECTOR_FIELD_H

#include "rmr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RMR_VECTOR_Q16_ONE 65536u
#define RMR_VECTOR_SQRT3_OVER_2_Q16 56756u
#define RMR_VECTOR_MOD_BASE 42u
#define RMR_VECTOR_ARC_BASE 360u
#define RMR_VECTOR_PHI_Q8_INIT 200u
#define RMR_VECTOR_PHI_Q8_MAX 255u
#define RMR_VECTOR_WATCHDOG_MAX 42u

#define RMR_VECTOR_FLAG_OK 0x00000001u
#define RMR_VECTOR_FLAG_ROLLBACK 0x00000002u
#define RMR_VECTOR_FLAG_WATCHDOG 0x00000004u
#define RMR_VECTOR_FLAG_VOID22 0x00000008u
#define RMR_VECTOR_FLAG_FAILSAFE 0x00000010u

#define RMR_VECTOR_OP_LOAD_NUM 0x60u
#define RMR_VECTOR_OP_MOD42 0x61u
#define RMR_VECTOR_OP_ARC360 0x62u
#define RMR_VECTOR_OP_CHORD_Q16 0x63u
#define RMR_VECTOR_OP_H_EQ_Q16 0x64u
#define RMR_VECTOR_OP_TOROID_NODE 0x65u
#define RMR_VECTOR_OP_CORRECT 0x66u
#define RMR_VECTOR_OP_AUDIT 0x67u
#define RMR_VECTOR_OP_SEAL 0x6Fu

typedef struct {
  u32 n_raw;
  u32 n_mod42;
  u32 arc_deg;
  u32 chord_q16;
  u32 h_q16;
  u32 toroid_node;
  u32 spiral_q16;
  u32 gap_q16;
  u32 audit_crc;
  u32 phi_q8;
  u32 flags;
  u32 watchdog;
} RmR_VectorFieldState;

void RmR_VectorField_Init(RmR_VectorFieldState *state);
u32 RmR_VectorField_RunIndex(RmR_VectorFieldState *state, u32 index, u32 correction_steps);
u32 RmR_VectorField_RunBytecode(RmR_VectorFieldState *state, const u8 *bytecode, u32 len);
u32 RmR_VectorField_Checksum(const RmR_VectorFieldState *state);
u32 RmR_VectorField_SmokeSignature(void);

#ifdef __cplusplus
}
#endif

#endif
