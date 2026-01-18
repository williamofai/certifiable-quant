/**
 * @file analyze.c
 * @project Certifiable-Quant
 * @brief Analysis module implementation (The Theorist)
 *
 * @traceability SRS-001-ANALYZE, CQ-MATH-001 §3
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * @license GPL-3.0 or Commercial (contact: william@fstopify.com)
 */

#include "analyze.h"
#include "sha256.h"
#include <math.h>
#include <string.h>
#include <float.h>

/* ============================================================================
 * FR-ANA-02: Overflow Proof Generation
 * ============================================================================ */

bool cq_compute_overflow_proof(uint32_t max_weight_mag,
                               uint32_t max_input_mag,
                               uint32_t dot_product_len,
                               cq_overflow_proof_t *proof)
{
    if (proof == NULL) {
        return false;
    }

    memset(proof, 0, sizeof(*proof));

    proof->max_weight_mag = max_weight_mag;
    proof->max_input_mag = max_input_mag;
    proof->dot_product_len = dot_product_len;

    /*
     * Safety condition (CQ-MATH-001 §3.4):
     * n × |w_int|_max × |x_int|_max < 2^63
     *
     * Compute in uint64_t to avoid overflow during check.
     */
    uint64_t n = (uint64_t)dot_product_len;
    uint64_t w = (uint64_t)max_weight_mag;
    uint64_t x = (uint64_t)max_input_mag;

    /* Check for overflow in the product itself */
    /* If any factor is zero, product is safe */
    if (n == 0 || w == 0 || x == 0) {
        proof->safety_margin = (uint64_t)1 << 63;
        proof->is_safe = true;
        return true;
    }

    /* Compute n * w first, checking for overflow */
    uint64_t nw;
    if (w > UINT64_MAX / n) {
        /* n * w would overflow uint64_t */
        proof->safety_margin = 0;
        proof->is_safe = false;
        return false;
    }
    nw = n * w;

    /* Now compute nw * x */
    uint64_t product;
    if (x > UINT64_MAX / nw) {
        /* nw * x would overflow uint64_t */
        proof->safety_margin = 0;
        proof->is_safe = false;
        return false;
    }
    product = nw * x;

    /* Check against 2^63 (signed int64_t max + 1) */
    const uint64_t limit = (uint64_t)1 << 63;

    if (product < limit) {
        proof->safety_margin = limit - product;
        proof->is_safe = true;
        return true;
    } else {
        proof->safety_margin = 0;
        proof->is_safe = false;
        return false;
    }
}

/* ============================================================================
 * FR-ANA-01: Range Propagation
 * ============================================================================ */

void cq_compute_weight_range(const float *weights,
                             size_t count,
                             cq_range_t *range)
{
    if (weights == NULL || range == NULL || count == 0) {
        if (range != NULL) {
            range->min_val = 0.0;
            range->max_val = 0.0;
        }
        return;
    }

    double min_val = (double)weights[0];
    double max_val = (double)weights[0];

    for (size_t i = 1; i < count; i++) {
        double v = (double)weights[i];
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
    }

    range->min_val = min_val;
    range->max_val = max_val;
}

void cq_propagate_range_linear(const cq_range_t *input_range,
                               const cq_range_t *weight_range,
                               const cq_range_t *bias_range,
                               uint32_t fan_in,
                               cq_range_t *output_range)
{
    if (input_range == NULL || weight_range == NULL || output_range == NULL) {
        if (output_range != NULL) {
            output_range->min_val = 0.0;
            output_range->max_val = 0.0;
        }
        return;
    }

    /*
     * Interval arithmetic for linear layer (CQ-MATH-001 §3.4):
     *
     * For y = w·x, the range of the product depends on signs:
     * [y_min, y_max] = hull of {w_min·x_min, w_min·x_max, w_max·x_min, w_max·x_max}
     *
     * For dot product of length n, we multiply by n (worst case).
     */
    double x_min = input_range->min_val;
    double x_max = input_range->max_val;
    double w_min = weight_range->min_val;
    double w_max = weight_range->max_val;

    /* Compute all four products */
    double p1 = w_min * x_min;
    double p2 = w_min * x_max;
    double p3 = w_max * x_min;
    double p4 = w_max * x_max;

    /* Find min and max of products */
    double prod_min = p1;
    double prod_max = p1;

    if (p2 < prod_min) prod_min = p2;
    if (p2 > prod_max) prod_max = p2;
    if (p3 < prod_min) prod_min = p3;
    if (p3 > prod_max) prod_max = p3;
    if (p4 < prod_min) prod_min = p4;
    if (p4 > prod_max) prod_max = p4;

    /* Scale by fan-in (number of accumulations) */
    double n = (double)fan_in;
    double y_min = prod_min * n;
    double y_max = prod_max * n;

    /* Add bias range if present */
    if (bias_range != NULL) {
        y_min += bias_range->min_val;
        y_max += bias_range->max_val;
    }

    output_range->min_val = y_min;
    output_range->max_val = y_max;
}

void cq_propagate_range_relu(const cq_range_t *input_range,
                             cq_range_t *output_range)
{
    if (input_range == NULL || output_range == NULL) {
        if (output_range != NULL) {
            output_range->min_val = 0.0;
            output_range->max_val = 0.0;
        }
        return;
    }

    /* ReLU: max(0, x) */
    output_range->min_val = (input_range->min_val > 0.0) ? input_range->min_val : 0.0;
    output_range->max_val = (input_range->max_val > 0.0) ? input_range->max_val : 0.0;
}

/* ============================================================================
 * FR-ANA-03: Operator Norm Computation
 * ============================================================================ */

double cq_frobenius_norm(const float *weights, size_t rows, size_t cols)
{
    if (weights == NULL || rows == 0 || cols == 0) {
        return 0.0;
    }

    double sum_sq = 0.0;
    size_t count = rows * cols;

    for (size_t i = 0; i < count; i++) {
        double w = (double)weights[i];
        sum_sq += w * w;
    }

    return sqrt(sum_sq);
}

double cq_row_sum_norm(const float *weights, size_t rows, size_t cols)
{
    if (weights == NULL || rows == 0 || cols == 0) {
        return 0.0;
    }

    double max_row_sum = 0.0;

    for (size_t i = 0; i < rows; i++) {
        double row_sum = 0.0;
        for (size_t j = 0; j < cols; j++) {
            double w = (double)weights[i * cols + j];
            row_sum += (w < 0) ? -w : w;  /* |w| */
        }
        if (row_sum > max_row_sum) {
            max_row_sum = row_sum;
        }
    }

    return max_row_sum;
}

/* ============================================================================
 * FR-ANA-04: Error Recurrence
 * ============================================================================ */

void cq_compute_error_contributions(cq_layer_contract_t *contract,
                                    double weight_scale,
                                    double output_scale,
                                    double max_input_norm)
{
    if (contract == NULL || weight_scale <= 0.0 || output_scale <= 0.0) {
        return;
    }

    /*
     * Error contributions (CQ-MATH-001 §3.6.2):
     *
     * weight_error_contrib = ‖ΔW_l‖ · ‖x_l‖ = (1/2S_w) × ‖x‖_max
     * bias_error_contrib   = ‖Δb_l‖ = 1/(2·S_acc) where S_acc = S_w × S_x
     * projection_error     = ε_proj,l = 1/(2·S_out)
     */

    contract->weight_error_contrib = (0.5 / weight_scale) * max_input_norm;
    contract->bias_error_contrib = 0.5 / (weight_scale * weight_scale);  /* Assuming S_x = S_w */
    contract->projection_error = 0.5 / output_scale;

    contract->local_error_sum = contract->weight_error_contrib +
                                contract->bias_error_contrib +
                                contract->projection_error;
}

void cq_apply_error_recurrence(cq_layer_contract_t *contract,
                               double input_error_bound)
{
    if (contract == NULL) {
        return;
    }

    contract->input_error_bound = input_error_bound;

    /*
     * Error recurrence (CQ-MATH-001 §3.6.2):
     * ε_{l+1} ≤ A_l·ε_l + local_error_sum
     */
    contract->output_error_bound = contract->amp_factor * input_error_bound +
                                   contract->local_error_sum;

    contract->is_valid = true;
}

/* ============================================================================
 * FR-ANA-05: Entry Error
 * ============================================================================ */

double cq_compute_entry_error(cq_scale_exp_t input_scale_exp)
{
    /*
     * Entry error (CQ-MATH-001 §3.6.1):
     * ε₀ ≤ 1/(2·S_in) where S_in = 2^exp
     */
    double scale = (double)(1LL << input_scale_exp);
    return 0.5 / scale;
}

/* ============================================================================
 * Context Management
 * ============================================================================ */

void cq_analysis_ctx_init(cq_analysis_ctx_t *ctx,
                          uint32_t layer_count,
                          cq_layer_contract_t *layers,
                          const cq_analyze_config_t *config)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));

    ctx->layer_count = layer_count;
    ctx->layers = layers;
    ctx->is_complete = false;
    ctx->is_valid = false;
    cq_fault_clear(&ctx->faults);

    if (config != NULL) {
        ctx->input_scale_exp = config->input_scale_exp;
        ctx->entry_error = cq_compute_entry_error(config->input_scale_exp);
    } else {
        ctx->input_scale_exp = 16;  /* Default Q16.16 */
        ctx->entry_error = cq_compute_entry_error(16);
    }
}

void cq_layer_contract_init(cq_layer_contract_t *contract,
                            uint32_t layer_index,
                            uint32_t layer_type,
                            uint32_t fan_in,
                            uint32_t fan_out)
{
    if (contract == NULL) {
        return;
    }

    memset(contract, 0, sizeof(*contract));

    contract->layer_index = layer_index;
    contract->layer_type = layer_type;
    contract->fan_in = fan_in;
    contract->fan_out = fan_out;
    contract->amp_factor = 1.0;  /* Default: no amplification */
    contract->is_valid = false;
}

/* ============================================================================
 * FR-ANA-06: Total Error & Digest
 * ============================================================================ */

int cq_compute_total_error(cq_analysis_ctx_t *ctx)
{
    if (ctx == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    if (ctx->layers == NULL || ctx->layer_count == 0) {
        /* No layers: total error = entry error */
        ctx->total_error_bound = ctx->entry_error;
        ctx->is_complete = true;
        ctx->is_valid = true;
        return 0;
    }

    /* Total error = output error of final layer */
    ctx->total_error_bound = ctx->layers[ctx->layer_count - 1].output_error_bound;

    /* Verify all layers are valid */
    ctx->is_valid = true;
    for (uint32_t i = 0; i < ctx->layer_count; i++) {
        if (!ctx->layers[i].is_valid) {
            ctx->is_valid = false;
            break;
        }
    }

    ctx->is_complete = true;

    return 0;
}

int cq_analysis_digest_generate(const cq_analysis_ctx_t *ctx,
                                cq_analysis_digest_t *digest)
{
    if (ctx == NULL || digest == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    memset(digest, 0, sizeof(*digest));

    digest->entry_error = ctx->entry_error;
    digest->total_error_bound = ctx->total_error_bound;
    digest->layer_count = ctx->layer_count;

    /* Count overflow-safe layers */
    uint32_t overflow_safe = 0;
    for (uint32_t i = 0; i < ctx->layer_count; i++) {
        if (ctx->layers != NULL && ctx->layers[i].overflow_proof.is_safe) {
            overflow_safe++;
        }
    }
    digest->overflow_safe_count = overflow_safe;

    /*
     * Compute SHA-256 of serialised layer contracts.
     * For now, we hash the raw contract structures.
     * In production, this would use canonical serialisation.
     */
    if (ctx->layers != NULL && ctx->layer_count > 0) {
        cq_sha256_ctx_t sha_ctx;
        cq_sha256_init(&sha_ctx);
        cq_sha256_update(&sha_ctx,
                         (const uint8_t *)ctx->layers,
                         ctx->layer_count * sizeof(cq_layer_contract_t));
        cq_sha256_final(&sha_ctx, digest->layers_hash);
    } else {
        /* Empty hash for no layers */
        memset(digest->layers_hash, 0, sizeof(digest->layers_hash));
    }

    return 0;
}
