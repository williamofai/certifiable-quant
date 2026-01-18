/**
 * @file test_convert.c
 * @brief Unit tests for convert module
 * Copyright (C) 2026 The Murray Family Innovation Trust
 */
/**
 * @file test_convert.c
 * @project Certifiable-Quant
 * @brief Unit tests for conversion module.
 *
 * @traceability SRS-003-CONVERT
 */

#include "convert.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { printf("  FAIL: %s\n", msg); return 1; } \
    else { tests_passed++; printf("  PASS: %s\n", msg); } \
} while(0)

/* ============================================================================
 * Test: RNE Quantization
 * ============================================================================ */

int test_rne_quantization(void) {
    printf("\n=== Test: RNE Quantization ===\n");
    cq_fault_flags_t f = {0};

    TEST(cq_quantize_weight_rne(1.5f, 1.0, &f) == 2, "1.5 -> 2");
    TEST(cq_quantize_weight_rne(2.5f, 1.0, &f) == 2, "2.5 -> 2");
    TEST(cq_quantize_weight_rne(3.5f, 1.0, &f) == 4, "3.5 -> 4");
    TEST(cq_quantize_weight_rne(-1.5f, 1.0, &f) == -2, "-1.5 -> -2");
    TEST(cq_quantize_weight_rne(-2.5f, 1.0, &f) == -2, "-2.5 -> -2");

    return 0;
}

/* ============================================================================
 * Test: Dyadic Constraint
 * ============================================================================ */

int test_dyadic_constraint(void) {
    printf("\n=== Test: Dyadic Constraint ===\n");
    cq_fault_flags_t f = {0};
    cq_layer_header_t h;
    memset(&h, 0, sizeof(h));

    h.weight_spec.is_symmetric = true;
    h.input_spec.is_symmetric = true;
    h.bias_spec.is_symmetric = true;
    h.weight_spec.scale_exp = 16;
    h.input_spec.scale_exp = 16;
    h.bias_spec.scale_exp = 32;

    TEST(cq_verify_constraints(&h, &f) == 0, "Valid dyadic passes");
    TEST(h.dyadic_valid == true, "dyadic_valid flag set");

    h.bias_spec.scale_exp = 16;
    TEST(cq_verify_constraints(&h, &f) == CQ_ERROR_DYADIC_VIOLATION,
         "Invalid dyadic fails");
    TEST(h.dyadic_valid == false, "dyadic_valid cleared");

    return 0;
}

/* ============================================================================
 * Test: Symmetric Enforcement
 * ============================================================================ */

int test_symmetric(void) {
    printf("\n=== Test: Symmetric Enforcement ===\n");
    cq_fault_flags_t f = {0};
    cq_tensor_spec_t s = {.is_symmetric = true};

    TEST(cq_verify_symmetric(&s, &f) == 0, "Symmetric passes");

    cq_fault_clear(&f);
    s.is_symmetric = false;
    TEST(cq_verify_symmetric(&s, &f) == CQ_FAULT_ASYMMETRIC_PARAMS,
         "Asymmetric fails");
    TEST(f.asymmetric == 1, "Fault flag set");

    return 0;
}

/* ============================================================================
 * Test: BatchNorm Folding
 * ============================================================================ */

int test_bn_folding(void) {
    printf("\n=== Test: BN Folding ===\n");

    float W[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[] = {0.5f, -0.5f};
    float mean[] = {1.0f, 2.0f};
    float var[] = {0.0f, 3.0f};
    float gamma[] = {2.0f, 4.0f};
    float beta[] = {0.0f, 10.0f};

    cq_bn_params_t bn = {
        .gamma = gamma, .beta = beta,
        .mean = mean, .var = var,
        .epsilon = 1.0f, .channel_count = 2
    };

    float W_out[4], b_out[2];
    cq_bn_folding_record_t rec;
    cq_fault_flags_t f = {0};

    int r = cq_fold_batchnorm(W, b, &bn, W_out, b_out, 2, 2, &rec, &f);

    TEST(r == 0, "Folding succeeds");
    TEST(rec.folding_occurred, "Record marked");
    TEST(fabs(W_out[0] - 2.0f) < 1e-5f, "W[0,0] correct");
    TEST(fabs(b_out[0] - (-1.0f)) < 1e-5f, "b[0] correct");

    return 0;
}

/* ============================================================================
 * Test: Batch Conversion
 * ============================================================================ */

int test_batch_convert(void) {
    printf("\n=== Test: Batch Conversion ===\n");

    float w[] = {1.0f, -1.0f, 0.5f, -0.5f};
    cq_fixed16_t q[4];
    cq_fault_flags_t f = {0};
    cq_tensor_spec_t s = {.scale_exp = 16, .is_symmetric = true};

    int r = cq_convert_weights(w, q, 4, &s, &f);

    TEST(r == 0, "Conversion succeeds");
    TEST(q[0] == 65536, "1.0 -> 65536");
    TEST(q[1] == -65536, "-1.0 -> -65536");
    TEST(q[2] == 32768, "0.5 -> 32768");
    TEST(q[3] == -32768, "-0.5 -> -32768");

    return 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("============================================\n");
    printf("Certifiable-Quant: Convert Module Tests\n");
    printf("============================================\n");

    int failed = 0;
    failed += test_rne_quantization();
    failed += test_dyadic_constraint();
    failed += test_symmetric();
    failed += test_bn_folding();
    failed += test_batch_convert();

    printf("\n============================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    printf("============================================\n");

    return failed ? 1 : 0;
}
