/**
 * @file bn_fold.c
 * @project Certifiable-Quant
 * @brief BatchNorm Folding Implementation
 *
 * @traceability SRS-003-CONVERT (FR-CNV-04), CQ-MATH-001 §8.2
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#include "convert.h"
#include "sha256.h"
#include <math.h>
#include <string.h>

/* ============================================================================
 * FR-CNV-04: BatchNorm Folding
 *
 * Mathematical derivation (CQ-MATH-001 §8.2):
 *   W' = W × γ / √(σ² + ε)
 *   b' = (b - μ) × γ / √(σ² + ε) + β
 * ============================================================================ */

int cq_fold_batchnorm(const float *W,
                      const float *b,
                      const cq_bn_params_t *bn,
                      float *W_folded,
                      float *b_folded,
                      size_t weight_rows,
                      size_t weight_cols,
                      cq_bn_folding_record_t *record,
                      cq_fault_flags_t *faults)
{
    if (!W || !bn || !W_folded || !b_folded || !record) {
        return CQ_ERROR_NULL_POINTER;
    }

    if (bn->channel_count != weight_rows) {
        return CQ_ERROR_DIMENSION_MISMATCH;
    }

    /* 1. Hash original BN parameters */
    {
        cq_sha256_ctx_t ctx;
        cq_sha256_init(&ctx);
        cq_sha256_update(&ctx, bn->gamma, bn->channel_count * sizeof(float));
        cq_sha256_update(&ctx, bn->beta, bn->channel_count * sizeof(float));
        cq_sha256_update(&ctx, bn->mean, bn->channel_count * sizeof(float));
        cq_sha256_update(&ctx, bn->var, bn->channel_count * sizeof(float));
        cq_sha256_update(&ctx, &bn->epsilon, sizeof(float));
        cq_sha256_final(&ctx, record->original_bn_hash);
    }

    /* 2. Perform folding (FP64 per IMPL-WATCH-03) */
    for (size_t i = 0; i < weight_rows; i++) {
        double var_eps = (double)bn->var[i] + (double)bn->epsilon;

        if (var_eps <= 0.0) {
            if (faults) faults->div_zero = 1;
            return CQ_FAULT_DIV_ZERO;
        }

        double inv_std = 1.0 / sqrt(var_eps);
        double scale = (double)bn->gamma[i] * inv_std;
        double offset = (double)bn->beta[i] - ((double)bn->mean[i] * scale);

        /* Fold bias */
        double old_b = b ? (double)b[i] : 0.0;
        b_folded[i] = (float)(old_b * scale + offset);

        /* Fold weights */
        for (size_t j = 0; j < weight_cols; j++) {
            size_t idx = i * weight_cols + j;
            W_folded[idx] = (float)((double)W[idx] * scale);
        }
    }

    /* 3. Hash folded parameters */
    {
        cq_sha256_ctx_t ctx;
        cq_sha256_init(&ctx);
        cq_sha256_update(&ctx, W_folded, weight_rows * weight_cols * sizeof(float));
        cq_sha256_update(&ctx, b_folded, weight_rows * sizeof(float));
        cq_sha256_final(&ctx, record->folded_weights_hash);
    }

    record->folding_occurred = true;
    return 0;
}
