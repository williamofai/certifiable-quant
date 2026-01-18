/**
 * @file convert.h
 * @project Certifiable-Quant
 * @brief Conversion module interface (The Transformer)
 *
 * @traceability SRS-003-CONVERT, CQ-MATH-001 §2-§3, §8
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#ifndef CQ_CONVERT_H
#define CQ_CONVERT_H

#include "cq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FR-CNV-01: Quantization Kernel (RNE)
 * ============================================================================ */

/**
 * @brief Quantize a single weight using RNE rounding.
 */
cq_fixed16_t cq_quantize_weight_rne(float w_fp,
                                    double scale,
                                    cq_fault_flags_t *faults);

/* ============================================================================
 * FR-CNV-02: Symmetric Enforcement
 * ============================================================================ */

/**
 * @brief Verify tensor uses symmetric quantization.
 */
int cq_verify_symmetric(const cq_tensor_spec_t *spec,
                        cq_fault_flags_t *faults);

/* ============================================================================
 * FR-CNV-03: Dyadic Constraint
 * ============================================================================ */

/**
 * @brief Verify dyadic constraint: S_bias == S_weight * S_input
 */
int cq_verify_constraints(cq_layer_header_t *hdr,
                          cq_fault_flags_t *faults);

/* ============================================================================
 * Batch Operations
 * ============================================================================ */

/**
 * @brief Batch convert weights from FP32 to Q16.16.
 */
int cq_convert_weights(const float *w_fp,
                       cq_fixed16_t *w_q,
                       size_t count,
                       const cq_tensor_spec_t *spec,
                       cq_fault_flags_t *faults);

/* ============================================================================
 * FR-CNV-04: BatchNorm Folding
 * ============================================================================ */

/**
 * @brief Fold BatchNorm into preceding linear/conv layer.
 */
int cq_fold_batchnorm(const float *W,
                      const float *b,
                      const cq_bn_params_t *bn,
                      float *W_folded,
                      float *b_folded,
                      size_t weight_rows,
                      size_t weight_cols,
                      cq_bn_folding_record_t *record,
                      cq_fault_flags_t *faults);

#ifdef __cplusplus
}
#endif

#endif /* CQ_CONVERT_H */
