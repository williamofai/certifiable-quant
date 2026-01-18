/**
 * @file verify.c
 * @project Certifiable-Quant
 * @brief Verification module implementation (The Judge)
 *
 * @traceability SRS-004-VERIFY, CQ-MATH-001 §7
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * @license GPL-3.0 or Commercial (contact: william@fstopify.com)
 */

#include "verify.h"
#include <math.h>
#include <string.h>

/* ============================================================================
 * FR-VER-02: Error Measurement
 * ============================================================================ */

double cq_linf_norm(const float *a, const float *b, size_t n)
{
    if (a == NULL || b == NULL || n == 0) {
        return 0.0;
    }

    double max_diff = 0.0;

    for (size_t i = 0; i < n; i++) {
        double diff = fabs((double)a[i] - (double)b[i]);
        if (diff > max_diff) {
            max_diff = diff;
        }
    }

    return max_diff;
}

double cq_linf_norm_q16(const float *fp, const cq_fixed16_t *q16, size_t n)
{
    if (fp == NULL || q16 == NULL || n == 0) {
        return 0.0;
    }

    double max_diff = 0.0;
    const double scale = 1.0 / (double)(1 << CQ_Q16_SHIFT);

    for (size_t i = 0; i < n; i++) {
        double q_float = (double)q16[i] * scale;
        double diff = fabs((double)fp[i] - q_float);
        if (diff > max_diff) {
            max_diff = diff;
        }
    }

    return max_diff;
}

/* ============================================================================
 * FR-VER-03, FR-VER-04: Bound Checking
 * ============================================================================ */

int cq_verify_check_bounds(cq_layer_comparison_t *layer,
                           cq_fault_flags_t *faults)
{
    if (layer == NULL || faults == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    /*
     * FR-VER-03: bound_satisfied = (error_max_measured ≤ error_bound_theoretical)
     * FR-VER-04: Set fault flag if violated
     */
    if (layer->error_max_measured > layer->error_bound_theoretical) {
        layer->bound_satisfied = false;
        faults->bound_violation = 1;
        return CQ_FAULT_BOUND_VIOLATION;
    }

    layer->bound_satisfied = true;
    return 0;
}

int cq_verify_check_all_bounds(cq_verification_report_t *report,
                               cq_fault_flags_t *faults)
{
    if (report == NULL || faults == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    int result = 0;
    report->all_bounds_satisfied = true;

    /* Check each layer */
    for (uint32_t i = 0; i < report->layer_count; i++) {
        if (report->layers == NULL) {
            return CQ_ERROR_NULL_POINTER;
        }

        int layer_result = cq_verify_check_bounds(&report->layers[i], faults);
        if (layer_result != 0) {
            report->all_bounds_satisfied = false;
            result = layer_result;
            /* Continue checking all layers to get complete picture */
        }
    }

    /* Check total bound */
    if (report->total_error_max_measured > report->total_error_theoretical) {
        report->total_bound_satisfied = false;
        faults->bound_violation = 1;
        result = CQ_FAULT_BOUND_VIOLATION;
    } else {
        report->total_bound_satisfied = true;
    }

    /* Merge faults into report */
    cq_fault_merge(&report->faults, faults);

    return result;
}

/* ============================================================================
 * FR-VER-05: Statistical Aggregation
 * ============================================================================ */

void cq_verify_layer_update(cq_layer_comparison_t *layer, double error)
{
    if (layer == NULL) {
        return;
    }

    /* Update count */
    layer->sample_count++;

    /* Update max */
    if (error > layer->error_max_measured) {
        layer->error_max_measured = error;
    }

    /* Update running sums for mean and std */
    layer->error_sum += error;
    layer->error_sum_sq += error * error;
}

void cq_verify_layer_finalize(cq_layer_comparison_t *layer)
{
    if (layer == NULL || layer->sample_count == 0) {
        return;
    }

    double n = (double)layer->sample_count;

    /* Compute mean */
    layer->error_mean_measured = layer->error_sum / n;

    /* Compute standard deviation (population std) */
    /* std = sqrt(E[X^2] - E[X]^2) */
    double mean_sq = layer->error_mean_measured * layer->error_mean_measured;
    double variance = (layer->error_sum_sq / n) - mean_sq;

    /* Guard against numerical issues */
    if (variance < 0.0) {
        variance = 0.0;
    }

    layer->error_std_measured = sqrt(variance);
}

void cq_verify_total_update(cq_verification_report_t *report, double error)
{
    if (report == NULL) {
        return;
    }

    /* Update count */
    report->sample_count++;

    /* Update max */
    if (error > report->total_error_max_measured) {
        report->total_error_max_measured = error;
    }

    /* Update running sums */
    report->total_error_sum += error;
    report->total_error_sum_sq += error * error;
}

void cq_verify_total_finalize(cq_verification_report_t *report)
{
    if (report == NULL || report->sample_count == 0) {
        return;
    }

    double n = (double)report->sample_count;

    /* Compute mean */
    report->total_error_mean = report->total_error_sum / n;

    /* Compute standard deviation */
    double mean_sq = report->total_error_mean * report->total_error_mean;
    double variance = (report->total_error_sum_sq / n) - mean_sq;

    if (variance < 0.0) {
        variance = 0.0;
    }

    report->total_error_std = sqrt(variance);
}

/* ============================================================================
 * Report Initialisation
 * ============================================================================ */

void cq_layer_comparison_init(cq_layer_comparison_t *layer,
                              uint32_t layer_index,
                              double bound)
{
    if (layer == NULL) {
        return;
    }

    memset(layer, 0, sizeof(*layer));
    layer->layer_index = layer_index;
    layer->error_bound_theoretical = bound;
    layer->bound_satisfied = false;
}

void cq_verification_report_init(cq_verification_report_t *report,
                                 uint32_t layer_count,
                                 cq_layer_comparison_t *layers,
                                 double total_bound)
{
    if (report == NULL) {
        return;
    }

    memset(report, 0, sizeof(*report));
    report->layer_count = layer_count;
    report->layers = layers;
    report->total_error_theoretical = total_bound;
    report->all_bounds_satisfied = false;
    report->total_bound_satisfied = false;
    cq_fault_clear(&report->faults);
}

/* ============================================================================
 * FR-VER-06: Digest Generation
 * ============================================================================ */

int cq_verification_digest_generate(const cq_verification_report_t *report,
                                    cq_verification_digest_t *digest)
{
    if (report == NULL || digest == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    memset(digest, 0, sizeof(*digest));

    /* Copy dataset hash */
    memcpy(digest->verification_set_hash,
           report->verification_set_hash,
           sizeof(digest->verification_set_hash));

    /* Copy counts */
    digest->sample_count = report->sample_count;

    /* Count layers that passed */
    uint32_t layers_passed = 0;
    for (uint32_t i = 0; i < report->layer_count; i++) {
        if (report->layers != NULL && report->layers[i].bound_satisfied) {
            layers_passed++;
        }
    }
    digest->layers_passed = layers_passed;

    /* Copy error values */
    digest->total_error_theoretical = report->total_error_theoretical;
    digest->total_error_max_measured = report->total_error_max_measured;

    /* Set pass/fail */
    digest->bounds_satisfied = (report->all_bounds_satisfied &&
                                report->total_bound_satisfied) ? 1 : 0;

    return 0;
}
