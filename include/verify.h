/**
 * @file verify.h
 * @project Certifiable-Quant
 * @brief Verification module interface (The Judge)
 *
 * Validates that quantized model meets theoretical error bounds.
 * Runs dual inference (FP32 + Q16), measures deviations, determines pass/fail.
 *
 * @traceability SRS-004-VERIFY, CQ-MATH-001 §7, CQ-STRUCT-001 §6
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * @license GPL-3.0 or Commercial (contact: william@fstopify.com)
 */

#ifndef CQ_VERIFY_H
#define CQ_VERIFY_H

#include "cq_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Verification configuration parameters.
 * @traceability SRS-004-VERIFY NFR-VER-03
 */
typedef struct {
    uint32_t min_samples;           /**< Minimum samples for valid verification */
    uint32_t max_samples;           /**< Maximum samples to process */
    bool     capture_intermediates; /**< Capture per-layer activations */
    bool     strict_mode;           /**< Fail on first bound violation */
    uint8_t  _reserved[2];          /**< Padding */
} cq_verify_config_t;

/**
 * @brief Default verification configuration.
 */
#define CQ_VERIFY_CONFIG_DEFAULT { \
    .min_samples = 100, \
    .max_samples = 1000, \
    .capture_intermediates = true, \
    .strict_mode = false, \
    ._reserved = {0, 0} \
}

/* ============================================================================
 * Layer Comparison (ST-006-A)
 * Traceability: CQ-STRUCT-001 §6.1
 * ============================================================================ */

/**
 * @brief Comparison result for a single layer.
 * @traceability CQ-MATH-001 §7.1, CQ-STRUCT-001 §6.1
 */
typedef struct {
    uint32_t layer_index;           /**< Layer index */
    uint32_t sample_count;          /**< Number of samples compared */
    
    /* Measured errors */
    double error_max_measured;      /**< Maximum measured error (L∞) */
    double error_mean_measured;     /**< Mean measured error */
    double error_std_measured;      /**< Standard deviation */
    
    /* Theoretical bound (from analysis) */
    double error_bound_theoretical; /**< ε_l from analysis */
    
    /* Running statistics (internal) */
    double error_sum;               /**< Sum for mean computation */
    double error_sum_sq;            /**< Sum of squares for std computation */
    
    /* Bound satisfaction */
    bool bound_satisfied;           /**< True if max_measured ≤ theoretical */
    uint8_t _reserved[7];           /**< Padding */
} cq_layer_comparison_t;

/* ============================================================================
 * Verification Report (ST-006-B)
 * Traceability: CQ-STRUCT-001 §6.2
 * ============================================================================ */

/**
 * @brief Complete verification report.
 * @traceability CQ-MATH-001 §7, CQ-STRUCT-001 §6.2
 */
typedef struct {
    /* Dataset identification */
    uint8_t verification_set_hash[32]; /**< SHA-256 of verification dataset */
    uint32_t sample_count;          /**< Number of verification samples */
    uint32_t layer_count;           /**< Number of layers verified */
    
    /* End-to-end results */
    double total_error_theoretical; /**< ε_total from analysis */
    double total_error_max_measured;/**< Maximum measured end-to-end error */
    double total_error_mean;        /**< Mean end-to-end error */
    double total_error_std;         /**< Std dev of end-to-end error */
    
    /* Running statistics (internal) */
    double total_error_sum;         /**< Sum for mean computation */
    double total_error_sum_sq;      /**< Sum of squares for std */
    
    /* Bound satisfaction */
    bool all_bounds_satisfied;      /**< True if all layers pass */
    bool total_bound_satisfied;     /**< True if total error passes */
    uint8_t _reserved[6];           /**< Padding */
    
    /* Per-layer comparisons (caller-allocated) */
    cq_layer_comparison_t *layers;  /**< Array of comparisons [layer_count] */
    
    /* Accumulated faults */
    cq_fault_flags_t faults;        /**< Accumulated fault flags */
    uint32_t _pad;                  /**< Padding */
} cq_verification_report_t;

/**
 * @brief Serialisable verification digest (for certificate).
 * @traceability CQ-MATH-001 §9.2, CQ-STRUCT-001 §6.2
 */
typedef struct {
    uint8_t verification_set_hash[32]; /**< Dataset hash */
    uint32_t sample_count;          /**< Number of samples */
    uint32_t layers_passed;         /**< Layers satisfying bounds */
    double total_error_theoretical; /**< ε_total claimed */
    double total_error_max_measured;/**< ε_max measured */
    uint8_t bounds_satisfied;       /**< 0 = fail, 1 = pass */
    uint8_t _reserved[7];           /**< Padding */
} cq_verification_digest_t;

/* ============================================================================
 * Error Measurement (FR-VER-02)
 * ============================================================================ */

/**
 * @brief Compute L-infinity (max absolute) norm of deviation.
 * 
 * @param a      First array (FP32 reference).
 * @param b      Second array (quantized, converted to float).
 * @param n      Array length.
 * @return       max_i |a[i] - b[i]|
 *
 * @traceability SRS-004-VERIFY FR-VER-02, CQ-MATH-001 §7.1
 */
double cq_linf_norm(const float *a, const float *b, size_t n);

/**
 * @brief Compute L-infinity norm between float and fixed-point arrays.
 * 
 * @param fp     Float array (FP32 reference).
 * @param q16    Fixed-point array (Q16.16).
 * @param n      Array length.
 * @return       max_i |fp[i] - Q16_TO_FLOAT(q16[i])|
 *
 * @traceability SRS-004-VERIFY FR-VER-02
 */
double cq_linf_norm_q16(const float *fp, const cq_fixed16_t *q16, size_t n);

/* ============================================================================
 * Bound Checking (FR-VER-03, FR-VER-04)
 * ============================================================================ */

/**
 * @brief Check if measured error satisfies theoretical bound.
 * 
 * @param layer   Layer comparison (updated with bound_satisfied).
 * @param faults  Output: Fault flags (bound_violation set if fails).
 * @return        0 if bound satisfied, CQ_FAULT_BOUND_VIOLATION if not.
 *
 * @traceability SRS-004-VERIFY FR-VER-03, FR-VER-04, CQ-MATH-001 §7.1, §7.2
 */
int cq_verify_check_bounds(cq_layer_comparison_t *layer,
                           cq_fault_flags_t *faults);

/**
 * @brief Check all layer bounds and total bound.
 * 
 * @param report  Verification report (updated with satisfaction flags).
 * @param faults  Output: Accumulated fault flags.
 * @return        0 if all bounds satisfied, CQ_FAULT_BOUND_VIOLATION if any fail.
 *
 * @traceability SRS-004-VERIFY FR-VER-03, FR-VER-04
 */
int cq_verify_check_all_bounds(cq_verification_report_t *report,
                               cq_fault_flags_t *faults);

/* ============================================================================
 * Statistical Aggregation (FR-VER-05)
 * ============================================================================ */

/**
 * @brief Update layer statistics with a new error sample.
 * 
 * @param layer   Layer comparison to update.
 * @param error   Measured error for this sample.
 *
 * @traceability SRS-004-VERIFY FR-VER-05
 */
void cq_verify_layer_update(cq_layer_comparison_t *layer, double error);

/**
 * @brief Finalise layer statistics (compute mean and std).
 * 
 * @param layer   Layer comparison to finalise.
 *
 * @traceability SRS-004-VERIFY FR-VER-05
 */
void cq_verify_layer_finalize(cq_layer_comparison_t *layer);

/**
 * @brief Update total (end-to-end) statistics with a new error sample.
 * 
 * @param report  Verification report to update.
 * @param error   Measured end-to-end error for this sample.
 *
 * @traceability SRS-004-VERIFY FR-VER-05
 */
void cq_verify_total_update(cq_verification_report_t *report, double error);

/**
 * @brief Finalise total statistics (compute mean and std).
 * 
 * @param report  Verification report to finalise.
 *
 * @traceability SRS-004-VERIFY FR-VER-05
 */
void cq_verify_total_finalize(cq_verification_report_t *report);

/* ============================================================================
 * Report Initialisation
 * ============================================================================ */

/**
 * @brief Initialise a layer comparison structure.
 * 
 * @param layer       Layer comparison to initialise.
 * @param layer_index Index of the layer.
 * @param bound       Theoretical error bound (from analysis).
 */
void cq_layer_comparison_init(cq_layer_comparison_t *layer,
                              uint32_t layer_index,
                              double bound);

/**
 * @brief Initialise a verification report structure.
 * 
 * @param report      Report to initialise.
 * @param layer_count Number of layers.
 * @param layers      Pre-allocated array of layer comparisons.
 * @param total_bound Theoretical total error bound (from analysis).
 */
void cq_verification_report_init(cq_verification_report_t *report,
                                 uint32_t layer_count,
                                 cq_layer_comparison_t *layers,
                                 double total_bound);

/* ============================================================================
 * Digest Generation (FR-VER-06)
 * ============================================================================ */

/**
 * @brief Generate verification digest for certificate.
 * 
 * @param report  Completed verification report.
 * @param digest  Output: Verification digest (fixed-size).
 * @return        0 on success, negative error code on failure.
 *
 * @pre  Verification complete (sample_count >= config.min_samples)
 *
 * @traceability SRS-004-VERIFY FR-VER-06, CQ-MATH-001 §9.2
 */
int cq_verification_digest_generate(const cq_verification_report_t *report,
                                    cq_verification_digest_t *digest);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert Q16.16 fixed-point to float.
 * @param q16  Fixed-point value.
 * @return     Float representation.
 */
static inline float cq_q16_to_float(cq_fixed16_t q16) {
    return (float)q16 / (float)(1 << CQ_Q16_SHIFT);
}

/**
 * @brief Check if verification passed.
 * @param report  Verification report.
 * @return        True if all bounds satisfied.
 */
static inline bool cq_verify_passed(const cq_verification_report_t *report) {
    return report->all_bounds_satisfied && report->total_bound_satisfied;
}

#ifdef __cplusplus
}
#endif

#endif /* CQ_VERIFY_H */
