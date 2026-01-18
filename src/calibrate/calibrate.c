/**
 * @file calibrate.c
 * @project Certifiable-Quant
 * @brief Calibration module implementation (The Observer)
 *
 * @traceability SRS-002-CALIBRATE, CQ-MATH-001 §5
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * @license GPL-3.0 or Commercial (contact: william@fstopify.com)
 */

#include "calibrate.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * FR-CAL-01: Statistics Collection
 * ============================================================================ */

void cq_tensor_stats_init(cq_tensor_stats_t *stats,
                          uint32_t tensor_id,
                          uint32_t layer_index,
                          float min_safe,
                          float max_safe)
{
    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));

    stats->tensor_id = tensor_id;
    stats->layer_index = layer_index;
    stats->min_safe = min_safe;
    stats->max_safe = max_safe;

    /* Initialise observed range to extremes for min/max tracking */
    stats->min_observed = FLT_MAX;
    stats->max_observed = -FLT_MAX;

    stats->coverage_ratio = 0.0f;
    stats->is_degenerate = false;
    stats->range_veto = false;
}

void cq_tensor_stats_update(cq_tensor_stats_t *stats,
                            const float *tensor,
                            size_t n)
{
    if (stats == NULL || tensor == NULL || n == 0) {
        return;
    }

    for (size_t i = 0; i < n; i++) {
        float v = tensor[i];

        /* Skip NaN and Inf values */
        if (isnan(v) || isinf(v)) {
            continue;
        }

        if (v < stats->min_observed) {
            stats->min_observed = v;
        }
        if (v > stats->max_observed) {
            stats->max_observed = v;
        }
    }
}

void cq_tensor_stats_update_single(cq_tensor_stats_t *stats, float value)
{
    if (stats == NULL || isnan(value) || isinf(value)) {
        return;
    }

    if (value < stats->min_observed) {
        stats->min_observed = value;
    }
    if (value > stats->max_observed) {
        stats->max_observed = value;
    }
}

/* ============================================================================
 * FR-CAL-02: Coverage Computation
 * ============================================================================ */

void cq_tensor_compute_coverage(cq_tensor_stats_t *stats,
                                const cq_calibrate_config_t *config)
{
    if (stats == NULL) {
        return;
    }

    float epsilon = (config != NULL) ? config->degenerate_epsilon : 1e-7f;

    /* Check for degenerate tensor first */
    float observed_range = stats->max_observed - stats->min_observed;
    float safe_range = stats->max_safe - stats->min_safe;

    /* Handle degenerate observed range */
    if (fabsf(observed_range) < epsilon) {
        stats->is_degenerate = true;
        stats->coverage_ratio = 1.0f;  /* Valid by definition */
        return;
    }

    /* Handle degenerate safe range (shouldn't happen, but be safe) */
    if (fabsf(safe_range) < epsilon) {
        stats->is_degenerate = true;
        stats->coverage_ratio = 1.0f;
        return;
    }

    stats->is_degenerate = false;

    /*
     * Coverage ratio (CQ-MATH-001 §5.1):
     * C_t = (U_obs - L_obs) / (U_safe - L_safe)
     */
    stats->coverage_ratio = observed_range / safe_range;
}

/* ============================================================================
 * FR-CAL-03: Range Veto Enforcement
 * ============================================================================ */

bool cq_tensor_check_range_veto(cq_tensor_stats_t *stats)
{
    if (stats == NULL) {
        return false;
    }

    /*
     * Range veto (CQ-MATH-001 §5.2):
     * Veto if [L_obs, U_obs] ⊄ [L_safe, U_safe]
     */
    if (stats->min_observed < stats->min_safe ||
        stats->max_observed > stats->max_safe) {
        stats->range_veto = true;
        return true;
    }

    stats->range_veto = false;
    return false;
}

/* ============================================================================
 * FR-CAL-05: Degenerate Tensor Handling
 * ============================================================================ */

bool cq_tensor_check_degenerate(cq_tensor_stats_t *stats, float epsilon)
{
    if (stats == NULL) {
        return false;
    }

    float range = stats->max_observed - stats->min_observed;

    if (fabsf(range) < epsilon) {
        stats->is_degenerate = true;
        return true;
    }

    stats->is_degenerate = false;
    return false;
}

/* ============================================================================
 * Report Management
 * ============================================================================ */

void cq_calibration_report_init(cq_calibration_report_t *report,
                                uint32_t tensor_count,
                                cq_tensor_stats_t *tensors)
{
    if (report == NULL) {
        return;
    }

    memset(report, 0, sizeof(*report));

    report->tensor_count = tensor_count;
    report->tensors = tensors;
    report->sample_count = 0;

    report->global_coverage_min = 0.0f;
    report->global_coverage_p10 = 0.0f;
    report->global_coverage_mean = 0.0f;

    report->range_veto_triggered = false;
    report->coverage_veto_triggered = false;

    cq_fault_clear(&report->faults);
}

int cq_calibration_report_finalize(cq_calibration_report_t *report,
                                   const cq_calibrate_config_t *config,
                                   cq_fault_flags_t *faults)
{
    if (report == NULL || config == NULL || faults == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    /* Compute coverage for each tensor */
    for (uint32_t i = 0; i < report->tensor_count; i++) {
        if (report->tensors == NULL) {
            return CQ_ERROR_NULL_POINTER;
        }

        cq_tensor_stats_t *t = &report->tensors[i];

        /* Compute coverage ratio */
        cq_tensor_compute_coverage(t, config);

        /* Check range veto */
        if (cq_tensor_check_range_veto(t)) {
            report->range_veto_triggered = true;
            faults->range_exceed = 1;
        }
    }

    /* Compute global coverage metrics */
    cq_calibration_compute_global_coverage(report);

    /* Check coverage thresholds */
    if (cq_calibration_check_coverage_threshold(report, config)) {
        report->coverage_veto_triggered = true;
        /* Coverage veto is warning only, not a fault */
    }

    /* Merge faults */
    cq_fault_merge(&report->faults, faults);

    return 0;
}

/* ============================================================================
 * FR-CAL-04: Coverage Threshold Enforcement
 * ============================================================================ */

/* Compare function for qsort */
int cq_float_compare_asc(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;

    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

void cq_calibration_compute_global_coverage(cq_calibration_report_t *report)
{
    if (report == NULL || report->tensors == NULL || report->tensor_count == 0) {
        return;
    }

    uint32_t n = report->tensor_count;

    /* Allocate temporary array for sorting (to compute percentile) */
    /* Note: For production, this should use static allocation */
    float *coverages = (float *)malloc(n * sizeof(float));
    if (coverages == NULL) {
        /* Fallback: compute min and mean only */
        float sum = 0.0f;
        float min_cov = FLT_MAX;

        for (uint32_t i = 0; i < n; i++) {
            float c = report->tensors[i].coverage_ratio;
            sum += c;
            if (c < min_cov) {
                min_cov = c;
            }
        }

        report->global_coverage_min = min_cov;
        report->global_coverage_mean = sum / (float)n;
        report->global_coverage_p10 = min_cov;  /* Fallback */
        return;
    }

    /* Collect coverage values */
    float sum = 0.0f;
    float min_cov = FLT_MAX;

    for (uint32_t i = 0; i < n; i++) {
        float c = report->tensors[i].coverage_ratio;
        coverages[i] = c;
        sum += c;
        if (c < min_cov) {
            min_cov = c;
        }
    }

    /* Compute mean */
    report->global_coverage_mean = sum / (float)n;
    report->global_coverage_min = min_cov;

    /* Sort for percentile computation */
    qsort(coverages, n, sizeof(float), cq_float_compare_asc);

    /* Compute 10th percentile (index = 0.1 * n) */
    uint32_t p10_idx = (uint32_t)((float)n * 0.1f);
    if (p10_idx >= n) {
        p10_idx = n - 1;
    }
    report->global_coverage_p10 = coverages[p10_idx];

    free(coverages);
}

bool cq_calibration_check_coverage_threshold(cq_calibration_report_t *report,
                                             const cq_calibrate_config_t *config)
{
    if (report == NULL || config == NULL) {
        return false;
    }

    /*
     * Coverage veto (SRS-002-CALIBRATE FR-CAL-04):
     * C_min >= threshold AND C_p10 >= threshold
     */
    if (report->global_coverage_min < config->coverage_min_threshold ||
        report->global_coverage_p10 < config->coverage_p10_threshold) {
        return true;  /* Veto triggered */
    }

    return false;
}

/* ============================================================================
 * FR-CAL-06: Digest Generation
 * ============================================================================ */

int cq_calibration_digest_generate(const cq_calibration_report_t *report,
                                   cq_calibration_digest_t *digest)
{
    if (report == NULL || digest == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    memset(digest, 0, sizeof(*digest));

    /* Copy dataset hash */
    memcpy(digest->dataset_hash, report->dataset_hash, sizeof(digest->dataset_hash));

    /* Copy counts */
    digest->sample_count = report->sample_count;
    digest->tensor_count = report->tensor_count;

    /* Copy coverage metrics */
    digest->global_coverage_min = report->global_coverage_min;
    digest->global_coverage_p10 = report->global_coverage_p10;

    /* Set veto status */
    digest->range_veto_status = report->range_veto_triggered ? 1 : 0;
    digest->coverage_veto_status = report->coverage_veto_triggered ? 1 : 0;

    return 0;
}
