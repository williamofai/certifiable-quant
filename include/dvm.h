/**
 * @file dvm.h
 * @project Certifiable-Quant
 * @brief DVM (Deterministic Virtual Machine) Primitive Operations
 *
 * @details Declares integer-only arithmetic primitives that guarantee
 *          bit-identical results across all platforms. These functions
 *          form the foundation of the certifiable quantization pipeline.
 *
 * @traceability CQ-MATH-001 §2-§3, CQ-STRUCT-001 §1
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#ifndef CQ_DVM_H
#define CQ_DVM_H

#include "cq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Saturation Operations (CQ-MATH-001 §2.4)
 * ============================================================================ */

/**
 * @brief Clamp 64-bit value to 32-bit range with fault signaling.
 */
int32_t cq_clamp32(int64_t x, cq_fault_flags_t *faults);

/**
 * @brief Saturating 64-bit addition.
 */
int64_t cq_add64_sat(int64_t a, int64_t b, cq_fault_flags_t *faults);

/**
 * @brief Saturating 64-bit subtraction.
 */
int64_t cq_sub64_sat(int64_t a, int64_t b, cq_fault_flags_t *faults);

/* ============================================================================
 * RNE Rounding (CQ-MATH-001 §2.3)
 * ============================================================================ */

/**
 * @brief Round-to-nearest-even right shift with saturation.
 * @param x     64-bit input value.
 * @param shift Number of bits to shift (0-62).
 * @param faults Fault flags output.
 * @return Rounded 32-bit result.
 */
int32_t cq_round_shift_rne(int64_t x, uint32_t shift, cq_fault_flags_t *faults);

/* ============================================================================
 * Fixed-Point Arithmetic (CQ-MATH-001 §3.3)
 * ============================================================================ */

/**
 * @brief Q16.16 multiplication with RNE rounding.
 */
cq_fixed16_t cq_mul_q16(cq_fixed16_t a, cq_fixed16_t b, cq_fault_flags_t *faults);

/**
 * @brief Q16.16 division with RNE rounding.
 */
cq_fixed16_t cq_div_q16(cq_fixed16_t a, cq_fixed16_t b, cq_fault_flags_t *faults);

/* ============================================================================
 * Accumulator Operations (CQ-MATH-001 §3.4)
 * ============================================================================ */

/**
 * @brief Multiply-accumulate: acc += a * b
 */
void cq_mac_q16(cq_accum64_t *acc, cq_fixed16_t a, cq_fixed16_t b,
                cq_fault_flags_t *faults);

/**
 * @brief Convert accumulator to Q16.16 with RNE rounding.
 */
cq_fixed16_t cq_acc_to_q16(cq_accum64_t acc, cq_fault_flags_t *faults);

/* ============================================================================
 * Overflow Safety (CQ-MATH-001 §3.4)
 * ============================================================================ */

/**
 * @brief Check if accumulator overflow is impossible for given parameters.
 */
bool cq_overflow_is_safe(const cq_overflow_proof_t *proof);

/* ============================================================================
 * Portable Arithmetic Shift
 * ============================================================================ */

/**
 * @brief Portable arithmetic right shift (32-bit).
 */
int32_t cq_sra32(int32_t v, int s);

/**
 * @brief Portable arithmetic right shift (64-bit).
 */
int64_t cq_sra64(int64_t v, int s);

#ifdef __cplusplus
}
#endif

#endif /* CQ_DVM_H */
