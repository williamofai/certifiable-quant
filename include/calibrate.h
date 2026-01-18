/**
 * @file calibrate.h
 * @project Certifiable-Quant
 * @brief Calibration module interface (The Observer)
 *
 * Collects runtime statistics from representative data to validate
 * theoretical assumptions and determine optimal quantization parameters.
 *
 * @traceability SRS-002-CALIBRATE, CQ-MATH-001 §5, CQ-STRUCT-001 §4
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * @license GPL-3.0 or Commercial (contact: william@fstopify.com)
 */

#ifndef CQ_CALIBRATE_H
#define CQ_CALIBRATE_H

#include "cq_types.h"
#include <stddef.h>
#include <float.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration (ST-004-B)
 * Traceability: CQ-STRUCT-001 §4.2
 * ============================================================================ */

/**
 * @brief Calibration configuration parameters.
 * @traceability SRS-002-CALIBRATE FR-CAL-04, FR-CAL-05, FR-CAL-07
 */
typedef struct {
    float coverage_min_threshold;   /**< C_min threshold (default: 0.90) */
    float coverage_p10_threshold;   /**< C_p10 threshold (default: 0.95) */
    float degenerate_epsilon;       /**< ε_degenerate for near-zero range (default: 1e-7) */
    uint32_t min_samples;           /**< Minimum calibration samples (default: 100) */
    uint32_t _reserved;             /**< Padding */
} cq_calibrate_config_t;

/**
 * @brief Default calibration configuration.
 */
#define CQ_CALIBRATE_CONFIG_DEFAULT { \
    .coverage_min_threshold = 0.90f, \
    .coverage_p10_threshold = 0.95f, \
    .degenerate_epsilon = 1e-7f, \
    .min_samples = 100, \
    ._reserved = 0 \
}

/* ============================================================================
 * Tensor Statistics (ST-004-A)
 * Traceability: CQ-MATH-001 §5.1, §5.2, CQ-STRUCT-001 §4.1
 * ============================================================================ */

/**
 * @brief Observed statistics for a single tensor.
 * @traceability SRS-002-CALIBRATE FR-CAL-01, FR-CAL-02, FR-CAL-03
 */
typedef struct {
    /* Tensor identification */
    uint32_t tensor_id;             /**< Unique tensor identifier */
    uint32_t layer_index;           /**< Parent layer index */

    /* Observed range (from calibration dataset) */
    float min_observed;             /**< L_obs: Minimum observed value */
    float max_observed;             /**< U_obs: Maximum observed value */

    /* Claimed safe range (for scaling) */
    float min_safe;                 /**< L_safe: Claimed minimum */
    float max_safe;                 /**< U_safe: Claimed maximum */

    /* Coverage metric */
    float coverage_ratio;           /**< C_t = observed_range / safe_range */

    /* Degenerate detection (§5.3) */
    bool is_degenerate;             /**< True if |max - min| < ε_degenerate */

    /* Range veto (§5.2) */
    bool range_veto;                /**< True if observed exceeds safe */

    uint8_t _reserved[2];           /**< Padding */
} cq_tensor_stats_t;

/* ============================================================================
 * Calibration Report (ST-004-C)
 * Traceability: CQ-MATH-001 §5.1, CQ-STRUCT-001 §4.3
 * ============================================================================ */

/**
 * @brief Complete calibration report.
 * @traceability SRS-002-CALIBRATE FR-CAL-02, FR-CAL-03, FR-CAL-04
 */
typedef struct {
    /* Dataset identification */
    uint8_t dataset_hash[32];       /**< SHA-256 of calibration dataset */
    uint32_t sample_count;          /**< Number of calibration samples */
    uint32_t tensor_count;          /**< Number of tensors calibrated */

    /* Global coverage metrics */
    float global_coverage_min;      /**< C_min: Minimum across all tensors */
    float global_coverage_p10;      /**< C_p10: 10th percentile */
    float global_coverage_mean;     /**< Mean coverage */
    uint32_t _pad1;                 /**< Padding */

    /* Veto status */
    bool range_veto_triggered;      /**< True if any tensor exceeds safe range */
    bool coverage_veto_triggered;   /**< True if C_min < threshold */
    uint8_t _reserved[6];           /**< Padding */

    /* Tensor statistics (caller-allocated) */
    cq_tensor_stats_t *tensors;     /**< Array of tensor stats [tensor_count] */

    /* Accumulated faults */
    cq_fault_flags_t faults;        /**< Accumulated fault flags */
    uint32_t _pad2;                 /**< Padding */
} cq_calibration_report_t;

/**
 * @brief Serialisable calibration digest (for certificate).
 * @traceability CQ-MATH-001 §9.2, CQ-STRUCT-001 §4.3
 */
typedef struct {
    uint8_t dataset_hash[32];       /**< SHA-256 of calibration dataset */
    uint32_t sample_count;          /**< Number of samples */
    uint32_t tensor_count;          /**< Number of tensors */
    float global_coverage_min;      /**< C_min */
    float global_coverage_p10;      /**< C_p10 */
    uint8_t range_veto_status;      /**< 0 = pass, 1 = veto */
    uint8_t coverage_veto_status;   /**< 0 = pass, 1 = veto */
    uint8_t _reserved[6];           /**< Padding */
} cq_calibration_digest_t;

/* ============================================================================
 * FR-CAL-01: Statistics Collection
 * ============================================================================ */

/**
 * @brief Initialise tensor statistics structure.
 *
 * @param stats       Tensor stats to initialise.
 * @param tensor_id   Unique tensor identifier.
 * @param layer_index Parent layer index.
 * @param min_safe    Claimed safe minimum.
 * @param max_safe    Claimed safe maximum.
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-01
 */
void cq_tensor_stats_init(cq_tensor_stats_t *stats,
                          uint32_t tensor_id,
                          uint32_t layer_index,
                          float min_safe,
                          float max_safe);

/**
 * @brief Update tensor statistics with observed values.
 *
 * @param stats   Tensor stats to update.
 * @param tensor  Observed tensor values.
 * @param n       Number of values in tensor.
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-01
 */
void cq_tensor_stats_update(cq_tensor_stats_t *stats,
                            const float *tensor,
                            size_t n);

/**
 * @brief Update tensor statistics with a single observed value.
 *
 * @param stats  Tensor stats to update.
 * @param value  Single observed value.
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-01
 */
void cq_tensor_stats_update_single(cq_tensor_stats_t *stats, float value);

/* ============================================================================
 * FR-CAL-02: Coverage Computation
 * ============================================================================ */

/**
 * @brief Compute coverage ratio for a tensor.
 *
 * @param stats   Tensor stats (coverage_ratio will be updated).
 * @param config  Calibration configuration (for degenerate epsilon).
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-02, FR-CAL-05
 */
void cq_tensor_compute_coverage(cq_tensor_stats_t *stats,
                                const cq_calibrate_config_t *config);

/* ============================================================================
 * FR-CAL-03: Range Veto Enforcement
 * ============================================================================ */

/**
 * @brief Check if tensor passes range veto.
 *
 * @param stats  Tensor stats (range_veto will be updated).
 * @return       true if veto triggered (observed exceeds safe).
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-03, CQ-MATH-001 §5.2
 */
bool cq_tensor_check_range_veto(cq_tensor_stats_t *stats);

/**
 * @brief Check range validity inline.
 */
static inline bool cq_tensor_range_valid(const cq_tensor_stats_t *stats) {
    return (stats->min_observed >= stats->min_safe) &&
           (stats->max_observed <= stats->max_safe);
}

/* ============================================================================
 * FR-CAL-05: Degenerate Tensor Handling
 * ============================================================================ */

/**
 * @brief Check if tensor is degenerate (near-zero range).
 *
 * @param stats   Tensor stats (is_degenerate will be updated).
 * @param epsilon Threshold for degenerate detection.
 * @return        true if tensor is degenerate.
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-05, CQ-MATH-001 §5.3
 */
bool cq_tensor_check_degenerate(cq_tensor_stats_t *stats, float epsilon);

/* ============================================================================
 * Report Management
 * ============================================================================ */

/**
 * @brief Initialise calibration report.
 *
 * @param report       Report to initialise.
 * @param tensor_count Number of tensors.
 * @param tensors      Pre-allocated array of tensor stats.
 *
 * @traceability SRS-002-CALIBRATE §5.1
 */
void cq_calibration_report_init(cq_calibration_report_t *report,
                                uint32_t tensor_count,
                                cq_tensor_stats_t *tensors);

/**
 * @brief Increment sample count in report.
 *
 * @param report  Report to update.
 */
static inline void cq_calibration_report_add_sample(cq_calibration_report_t *report) {
    if (report != NULL) {
        report->sample_count++;
    }
}

/**
 * @brief Finalise calibration report (compute global metrics).
 *
 * @param report  Report to finalise.
 * @param config  Calibration configuration.
 * @param faults  Output: Accumulated fault flags.
 * @return        0 on success, negative error code on failure.
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-02, FR-CAL-03, FR-CAL-04
 */
int cq_calibration_report_finalize(cq_calibration_report_t *report,
                                   const cq_calibrate_config_t *config,
                                   cq_fault_flags_t *faults);

/* ============================================================================
 * FR-CAL-04: Coverage Threshold Enforcement
 * ============================================================================ */

/**
 * @brief Compute global coverage metrics from tensor stats.
 *
 * @param report  Report with tensor stats populated.
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-02, FR-CAL-04
 */
void cq_calibration_compute_global_coverage(cq_calibration_report_t *report);

/**
 * @brief Check coverage thresholds.
 *
 * @param report  Report with global coverage computed.
 * @param config  Configuration with thresholds.
 * @return        true if coverage veto triggered.
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-04
 */
bool cq_calibration_check_coverage_threshold(cq_calibration_report_t *report,
                                             const cq_calibrate_config_t *config);

/* ============================================================================
 * FR-CAL-06: Digest Generation
 * ============================================================================ */

/**
 * @brief Generate calibration digest for certificate.
 *
 * @param report  Completed calibration report.
 * @param digest  Output: Calibration digest (fixed-size).
 * @return        0 on success, negative error code on failure.
 *
 * @pre  report->range_veto_triggered == false (for valid certificate)
 *
 * @traceability SRS-002-CALIBRATE FR-CAL-06, CQ-MATH-001 §9.2
 */
int cq_calibration_digest_generate(const cq_calibration_report_t *report,
                                   cq_calibration_digest_t *digest);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if calibration passed (no range veto).
 */
static inline bool cq_calibration_passed(const cq_calibration_report_t *report) {
    return !report->range_veto_triggered;
}

/**
 * @brief Check if calibration passed with good coverage.
 */
static inline bool cq_calibration_passed_full(const cq_calibration_report_t *report) {
    return !report->range_veto_triggered && !report->coverage_veto_triggered;
}

/**
 * @brief Compare function for qsort (ascending float).
 */
int cq_float_compare_asc(const void *a, const void *b);

#ifdef __cplusplus
}
#endif

#endif /* CQ_CALIBRATE_H */
