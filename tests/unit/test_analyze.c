/**
 * @file test_analyze.c
 * @project Certifiable-Quant
 * @brief Unit tests for analysis module
 *
 * @traceability SRS-001-ANALYZE TC-ANA-01 through TC-ANA-07
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#include "analyze.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static int name(void)
#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        return 0; \
    } \
} while(0)
#define ASSERT_NEAR(a, b, tol, msg) do { \
    if (fabs((double)(a) - (double)(b)) > (tol)) { \
        printf("  FAIL: %s (got %g, expected %g)\n", msg, (double)(a), (double)(b)); \
        return 0; \
    } \
} while(0)
#define RUN_TEST(test) do { \
    printf("Running %s...\n", #test); \
    tests_run++; \
    if (test()) { \
        tests_passed++; \
        printf("  PASS\n"); \
    } \
} while(0)

/* ============================================================================
 * TC-ANA-02: Overflow Proof Tests
 * ============================================================================ */

TEST(test_overflow_proof_safe_small)
{
    cq_overflow_proof_t proof;

    /* Small values: definitely safe */
    bool result = cq_compute_overflow_proof(100, 100, 100, &proof);

    ASSERT(result == true, "small values should be safe");
    ASSERT(proof.is_safe == true, "is_safe should be true");
    ASSERT(proof.max_weight_mag == 100, "weight mag should match");
    ASSERT(proof.max_input_mag == 100, "input mag should match");
    ASSERT(proof.dot_product_len == 100, "dot product len should match");
    ASSERT(proof.safety_margin > 0, "safety margin should be positive");
    return 1;
}

TEST(test_overflow_proof_safe_q16_typical)
{
    cq_overflow_proof_t proof;

    /*
     * Typical Q16.16 scenario (CQ-MATH-001 §3.4):
     * Weight range: [-2^15, 2^15-1], Input range: [-2^15, 2^15-1]
     * Max safe n: 2^33
     */
    uint32_t w_max = 32767;   /* 2^15 - 1 */
    uint32_t x_max = 32767;
    uint32_t n = 1024;        /* Typical layer size */

    bool result = cq_compute_overflow_proof(w_max, x_max, n, &proof);

    ASSERT(result == true, "typical Q16.16 should be safe");
    ASSERT(proof.is_safe == true, "is_safe should be true");
    return 1;
}

TEST(test_overflow_proof_unsafe_large_n)
{
    cq_overflow_proof_t proof;

    /*
     * Unsafe: use full 32-bit magnitudes
     * n × w × x where all are large
     * 2^31 × 2^31 × 2 = 2^63 (exactly at limit, unsafe)
     */
    uint32_t w_max = 0x80000000;  /* 2^31 */
    uint32_t x_max = 0x80000000;  /* 2^31 */
    uint32_t n = 2;

    bool result = cq_compute_overflow_proof(w_max, x_max, n, &proof);

    ASSERT(result == false, "2^63 product should be unsafe");
    ASSERT(proof.is_safe == false, "is_safe should be false");
    return 1;
}

TEST(test_overflow_proof_zero_values)
{
    cq_overflow_proof_t proof;

    /* Zero weight: always safe */
    bool result = cq_compute_overflow_proof(0, 1000, 1000, &proof);
    ASSERT(result == true, "zero weight should be safe");
    ASSERT(proof.is_safe == true, "is_safe should be true");

    /* Zero input: always safe */
    result = cq_compute_overflow_proof(1000, 0, 1000, &proof);
    ASSERT(result == true, "zero input should be safe");

    /* Zero n: always safe */
    result = cq_compute_overflow_proof(1000, 1000, 0, &proof);
    ASSERT(result == true, "zero n should be safe");

    return 1;
}

TEST(test_overflow_proof_boundary)
{
    cq_overflow_proof_t proof;

    /*
     * Test near the boundary: n × w × x = 2^62 (safe)
     * 2^62 = 4611686018427387904
     * Use n=2^20, w=2^21, x=2^21 → product = 2^62
     */
    uint32_t w_max = 1 << 21;  /* 2^21 */
    uint32_t x_max = 1 << 21;
    uint32_t n = 1 << 20;      /* 2^20 */

    bool result = cq_compute_overflow_proof(w_max, x_max, n, &proof);

    ASSERT(result == true, "2^62 should be safe");
    ASSERT(proof.is_safe == true, "is_safe should be true");
    ASSERT(proof.safety_margin > 0, "should have some margin");
    return 1;
}

/* ============================================================================
 * TC-ANA-01: Range Propagation Tests
 * ============================================================================ */

TEST(test_weight_range_basic)
{
    float weights[] = {1.0f, -2.0f, 3.0f, -4.0f, 5.0f};
    cq_range_t range;

    cq_compute_weight_range(weights, 5, &range);

    ASSERT_NEAR(range.min_val, -4.0, 1e-6, "min should be -4.0");
    ASSERT_NEAR(range.max_val, 5.0, 1e-6, "max should be 5.0");
    return 1;
}

TEST(test_weight_range_all_positive)
{
    float weights[] = {1.0f, 2.0f, 3.0f};
    cq_range_t range;

    cq_compute_weight_range(weights, 3, &range);

    ASSERT_NEAR(range.min_val, 1.0, 1e-6, "min should be 1.0");
    ASSERT_NEAR(range.max_val, 3.0, 1e-6, "max should be 3.0");
    return 1;
}

TEST(test_weight_range_single)
{
    float weights[] = {42.0f};
    cq_range_t range;

    cq_compute_weight_range(weights, 1, &range);

    ASSERT_NEAR(range.min_val, 42.0, 1e-6, "min should be 42.0");
    ASSERT_NEAR(range.max_val, 42.0, 1e-6, "max should be 42.0");
    return 1;
}

TEST(test_range_propagation_positive)
{
    /* Input: [0, 1], Weight: [0.5, 1.0], Bias: [0, 0], fan_in = 2 */
    cq_range_t input = {0.0, 1.0};
    cq_range_t weight = {0.5, 1.0};
    cq_range_t bias = {0.0, 0.0};
    cq_range_t output;

    cq_propagate_range_linear(&input, &weight, &bias, 2, &output);

    /*
     * Products: 0.5×0=0, 0.5×1=0.5, 1.0×0=0, 1.0×1=1.0
     * min=0, max=1.0
     * × fan_in=2: [0, 2.0]
     */
    ASSERT_NEAR(output.min_val, 0.0, 1e-6, "output min should be 0");
    ASSERT_NEAR(output.max_val, 2.0, 1e-6, "output max should be 2.0");
    return 1;
}

TEST(test_range_propagation_mixed_signs)
{
    /* Input: [-1, 1], Weight: [-1, 1], fan_in = 3 */
    cq_range_t input = {-1.0, 1.0};
    cq_range_t weight = {-1.0, 1.0};
    cq_range_t output;

    cq_propagate_range_linear(&input, &weight, NULL, 3, &output);

    /*
     * Products: (-1)×(-1)=1, (-1)×1=-1, 1×(-1)=-1, 1×1=1
     * min=-1, max=1
     * × fan_in=3: [-3, 3]
     */
    ASSERT_NEAR(output.min_val, -3.0, 1e-6, "output min should be -3.0");
    ASSERT_NEAR(output.max_val, 3.0, 1e-6, "output max should be 3.0");
    return 1;
}

TEST(test_range_propagation_with_bias)
{
    cq_range_t input = {0.0, 1.0};
    cq_range_t weight = {1.0, 1.0};  /* Constant weight */
    cq_range_t bias = {-0.5, 0.5};
    cq_range_t output;

    cq_propagate_range_linear(&input, &weight, &bias, 1, &output);

    /* Products: 1×0=0, 1×1=1 → [0, 1] */
    /* + bias [-0.5, 0.5] → [-0.5, 1.5] */
    ASSERT_NEAR(output.min_val, -0.5, 1e-6, "output min with bias");
    ASSERT_NEAR(output.max_val, 1.5, 1e-6, "output max with bias");
    return 1;
}

TEST(test_range_relu_positive)
{
    cq_range_t input = {1.0, 5.0};
    cq_range_t output;

    cq_propagate_range_relu(&input, &output);

    ASSERT_NEAR(output.min_val, 1.0, 1e-6, "relu min should stay 1.0");
    ASSERT_NEAR(output.max_val, 5.0, 1e-6, "relu max should stay 5.0");
    return 1;
}

TEST(test_range_relu_negative)
{
    cq_range_t input = {-5.0, -1.0};
    cq_range_t output;

    cq_propagate_range_relu(&input, &output);

    ASSERT_NEAR(output.min_val, 0.0, 1e-6, "relu clamps to 0");
    ASSERT_NEAR(output.max_val, 0.0, 1e-6, "relu clamps to 0");
    return 1;
}

TEST(test_range_relu_mixed)
{
    cq_range_t input = {-2.0, 3.0};
    cq_range_t output;

    cq_propagate_range_relu(&input, &output);

    ASSERT_NEAR(output.min_val, 0.0, 1e-6, "relu min clamps to 0");
    ASSERT_NEAR(output.max_val, 3.0, 1e-6, "relu max stays 3.0");
    return 1;
}

/* ============================================================================
 * TC-ANA-03: Operator Norm Tests
 * ============================================================================ */

TEST(test_frobenius_norm_identity)
{
    /* 2x2 identity matrix */
    float I[] = {1.0f, 0.0f, 0.0f, 1.0f};

    double norm = cq_frobenius_norm(I, 2, 2);

    /* ‖I‖_F = sqrt(1 + 1) = sqrt(2) ≈ 1.414 */
    ASSERT_NEAR(norm, sqrt(2.0), 1e-6, "identity Frobenius norm");
    return 1;
}

TEST(test_frobenius_norm_ones)
{
    /* 2x3 matrix of ones */
    float M[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    double norm = cq_frobenius_norm(M, 2, 3);

    /* ‖M‖_F = sqrt(6) ≈ 2.449 */
    ASSERT_NEAR(norm, sqrt(6.0), 1e-6, "ones matrix Frobenius norm");
    return 1;
}

TEST(test_frobenius_norm_known)
{
    /* Known matrix: [[3, 4], [0, 0]] */
    float M[] = {3.0f, 4.0f, 0.0f, 0.0f};

    double norm = cq_frobenius_norm(M, 2, 2);

    /* ‖M‖_F = sqrt(9 + 16) = 5 */
    ASSERT_NEAR(norm, 5.0, 1e-6, "3-4-5 triangle Frobenius norm");
    return 1;
}

TEST(test_row_sum_norm)
{
    /* Matrix: [[1, -2, 3], [4, 5, -6]] */
    float M[] = {1.0f, -2.0f, 3.0f, 4.0f, 5.0f, -6.0f};

    double norm = cq_row_sum_norm(M, 2, 3);

    /* Row sums: |1|+|-2|+|3| = 6, |4|+|5|+|-6| = 15 */
    /* Max = 15 */
    ASSERT_NEAR(norm, 15.0, 1e-6, "row sum norm");
    return 1;
}

/* ============================================================================
 * TC-ANA-04: Error Recurrence Tests
 * ============================================================================ */

TEST(test_error_contributions)
{
    cq_layer_contract_t contract;
    cq_layer_contract_init(&contract, 0, CQ_LAYER_LINEAR, 100, 50);

    /* Q16.16: scale = 2^16 = 65536 */
    double weight_scale = 65536.0;
    double output_scale = 65536.0;
    double max_input_norm = 10.0;

    cq_compute_error_contributions(&contract, weight_scale, output_scale, max_input_norm);

    /* weight_error_contrib = (0.5 / 65536) × 10 ≈ 7.63e-5 */
    ASSERT_NEAR(contract.weight_error_contrib, 7.629e-5, 1e-7, "weight error");

    /* projection_error = 0.5 / 65536 ≈ 7.63e-6 */
    ASSERT_NEAR(contract.projection_error, 7.629e-6, 1e-8, "projection error");

    /* local_error_sum should be sum of all contributions */
    ASSERT(contract.local_error_sum > 0, "local error sum should be positive");
    return 1;
}

TEST(test_error_recurrence_single_layer)
{
    cq_layer_contract_t contract;
    cq_layer_contract_init(&contract, 0, CQ_LAYER_LINEAR, 100, 50);

    contract.amp_factor = 2.0;
    contract.local_error_sum = 0.001;

    double entry_error = 0.0001;  /* ε₀ */

    cq_apply_error_recurrence(&contract, entry_error);

    /* ε₁ = A × ε₀ + local = 2 × 0.0001 + 0.001 = 0.0012 */
    ASSERT_NEAR(contract.input_error_bound, 0.0001, 1e-10, "input error bound");
    ASSERT_NEAR(contract.output_error_bound, 0.0012, 1e-10, "output error bound");
    ASSERT(contract.is_valid == true, "contract should be valid");
    return 1;
}

TEST(test_error_recurrence_chain)
{
    /* Simulate 3-layer network */
    cq_layer_contract_t layers[3];

    for (int i = 0; i < 3; i++) {
        cq_layer_contract_init(&layers[i], (uint32_t)i, CQ_LAYER_LINEAR, 100, 100);
        layers[i].amp_factor = 1.5;       /* 50% amplification per layer */
        layers[i].local_error_sum = 0.001; /* Fixed local error */
    }

    double entry_error = 0.0001;

    /* Propagate through layers */
    cq_apply_error_recurrence(&layers[0], entry_error);
    cq_apply_error_recurrence(&layers[1], layers[0].output_error_bound);
    cq_apply_error_recurrence(&layers[2], layers[1].output_error_bound);

    /* Layer 0: ε₁ = 1.5 × 0.0001 + 0.001 = 0.00115 */
    ASSERT_NEAR(layers[0].output_error_bound, 0.00115, 1e-8, "layer 0 output");

    /* Layer 1: ε₂ = 1.5 × 0.00115 + 0.001 = 0.002725 */
    ASSERT_NEAR(layers[1].output_error_bound, 0.002725, 1e-8, "layer 1 output");

    /* Layer 2: ε₃ = 1.5 × 0.002725 + 0.001 = 0.0050875 */
    ASSERT_NEAR(layers[2].output_error_bound, 0.0050875, 1e-8, "layer 2 output");

    return 1;
}

/* ============================================================================
 * TC-ANA-05: Entry Error Tests
 * ============================================================================ */

TEST(test_entry_error_q16)
{
    /* Q16.16: ε₀ = 1/(2 × 2^16) = 2^(-17) */
    double entry = cq_compute_entry_error(16);

    ASSERT_NEAR(entry, 7.62939453125e-6, 1e-12, "Q16.16 entry error");
    return 1;
}

TEST(test_entry_error_q24)
{
    /* Q8.24: ε₀ = 1/(2 × 2^24) = 2^(-25) */
    double entry = cq_compute_entry_error(24);

    ASSERT_NEAR(entry, 2.98023223876953125e-8, 1e-14, "Q8.24 entry error");
    return 1;
}

/* ============================================================================
 * TC-ANA-06: Total Error & Context Tests
 * ============================================================================ */

TEST(test_analysis_ctx_init)
{
    cq_analysis_ctx_t ctx;
    cq_layer_contract_t layers[2];
    cq_analyze_config_t config = CQ_ANALYZE_CONFIG_DEFAULT;

    cq_analysis_ctx_init(&ctx, 2, layers, &config);

    ASSERT(ctx.layer_count == 2, "layer count should be 2");
    ASSERT(ctx.layers == layers, "layers pointer should match");
    ASSERT(ctx.input_scale_exp == 16, "default scale exp should be 16");
    ASSERT(ctx.is_complete == false, "should not be complete yet");
    ASSERT(ctx.is_valid == false, "should not be valid yet");
    ASSERT_NEAR(ctx.entry_error, 7.629e-6, 1e-8, "entry error for Q16.16");
    return 1;
}

TEST(test_compute_total_error)
{
    cq_analysis_ctx_t ctx;
    cq_layer_contract_t layers[2];
    cq_analyze_config_t config = CQ_ANALYZE_CONFIG_DEFAULT;

    cq_analysis_ctx_init(&ctx, 2, layers, &config);

    /* Set up layers */
    cq_layer_contract_init(&layers[0], 0, CQ_LAYER_LINEAR, 100, 50);
    cq_layer_contract_init(&layers[1], 1, CQ_LAYER_LINEAR, 50, 10);

    layers[0].amp_factor = 1.0;
    layers[0].local_error_sum = 0.001;
    layers[1].amp_factor = 1.0;
    layers[1].local_error_sum = 0.001;

    /* Apply recurrence */
    cq_apply_error_recurrence(&layers[0], ctx.entry_error);
    cq_apply_error_recurrence(&layers[1], layers[0].output_error_bound);

    /* Compute total */
    int result = cq_compute_total_error(&ctx);

    ASSERT(result == 0, "compute total should succeed");
    ASSERT(ctx.is_complete == true, "should be complete");
    ASSERT(ctx.is_valid == true, "should be valid");
    ASSERT_NEAR(ctx.total_error_bound, layers[1].output_error_bound, 1e-12,
                "total should equal final layer output");
    return 1;
}

TEST(test_analysis_passed_helper)
{
    cq_analysis_ctx_t ctx;
    cq_layer_contract_t layers[1];

    cq_analysis_ctx_init(&ctx, 1, layers, NULL);

    ctx.is_complete = false;
    ctx.is_valid = false;
    ASSERT(cq_analysis_passed(&ctx) == false, "incomplete should not pass");

    ctx.is_complete = true;
    ctx.is_valid = true;
    ASSERT(cq_analysis_passed(&ctx) == true, "complete+valid should pass");

    ctx.faults.bound_violation = 1;
    ASSERT(cq_analysis_passed(&ctx) == false, "fatal fault should not pass");

    return 1;
}

/* ============================================================================
 * TC-ANA-06: Digest Generation Tests
 * ============================================================================ */

TEST(test_digest_generation)
{
    cq_analysis_ctx_t ctx;
    cq_layer_contract_t layers[2];
    cq_analysis_digest_t digest;
    cq_analyze_config_t config = CQ_ANALYZE_CONFIG_DEFAULT;

    cq_analysis_ctx_init(&ctx, 2, layers, &config);

    /* Set up layers with overflow proofs */
    cq_layer_contract_init(&layers[0], 0, CQ_LAYER_LINEAR, 100, 50);
    cq_layer_contract_init(&layers[1], 1, CQ_LAYER_LINEAR, 50, 10);

    layers[0].overflow_proof.is_safe = true;
    layers[1].overflow_proof.is_safe = true;
    layers[0].is_valid = true;
    layers[1].is_valid = true;
    layers[1].output_error_bound = 0.005;

    ctx.total_error_bound = 0.005;

    int result = cq_analysis_digest_generate(&ctx, &digest);

    ASSERT(result == 0, "digest generation should succeed");
    ASSERT_NEAR(digest.entry_error, ctx.entry_error, 1e-12, "entry error should match");
    ASSERT_NEAR(digest.total_error_bound, 0.005, 1e-12, "total error should match");
    ASSERT(digest.layer_count == 2, "layer count should match");
    ASSERT(digest.overflow_safe_count == 2, "both layers should be overflow safe");

    /* Hash should be non-zero (actual value depends on layer content) */
    int hash_nonzero = 0;
    for (int i = 0; i < 32; i++) {
        if (digest.layers_hash[i] != 0) {
            hash_nonzero = 1;
            break;
        }
    }
    ASSERT(hash_nonzero, "layers hash should be non-zero");

    return 1;
}

TEST(test_digest_null_inputs)
{
    cq_analysis_ctx_t ctx;
    cq_analysis_digest_t digest;

    int result1 = cq_analysis_digest_generate(NULL, &digest);
    int result2 = cq_analysis_digest_generate(&ctx, NULL);

    ASSERT(result1 == CQ_ERROR_NULL_POINTER, "NULL ctx should error");
    ASSERT(result2 == CQ_ERROR_NULL_POINTER, "NULL digest should error");
    return 1;
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST(test_range_magnitude)
{
    cq_range_t r1 = {-5.0, 3.0};
    cq_range_t r2 = {-2.0, 7.0};
    cq_range_t r3 = {1.0, 4.0};

    ASSERT_NEAR(cq_range_magnitude(&r1), 5.0, 1e-10, "magnitude of [-5, 3]");
    ASSERT_NEAR(cq_range_magnitude(&r2), 7.0, 1e-10, "magnitude of [-2, 7]");
    ASSERT_NEAR(cq_range_magnitude(&r3), 4.0, 1e-10, "magnitude of [1, 4]");
    return 1;
}

TEST(test_scale_from_exp)
{
    ASSERT_NEAR(cq_scale_from_exp(0), 1.0, 1e-10, "2^0 = 1");
    ASSERT_NEAR(cq_scale_from_exp(16), 65536.0, 1e-10, "2^16 = 65536");
    ASSERT_NEAR(cq_scale_from_exp(24), 16777216.0, 1e-10, "2^24 = 16777216");
    return 1;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    printf("=== Certifiable-Quant Analyze Module Tests ===\n\n");

    /* Overflow proof tests */
    RUN_TEST(test_overflow_proof_safe_small);
    RUN_TEST(test_overflow_proof_safe_q16_typical);
    RUN_TEST(test_overflow_proof_unsafe_large_n);
    RUN_TEST(test_overflow_proof_zero_values);
    RUN_TEST(test_overflow_proof_boundary);

    /* Range propagation tests */
    RUN_TEST(test_weight_range_basic);
    RUN_TEST(test_weight_range_all_positive);
    RUN_TEST(test_weight_range_single);
    RUN_TEST(test_range_propagation_positive);
    RUN_TEST(test_range_propagation_mixed_signs);
    RUN_TEST(test_range_propagation_with_bias);
    RUN_TEST(test_range_relu_positive);
    RUN_TEST(test_range_relu_negative);
    RUN_TEST(test_range_relu_mixed);

    /* Operator norm tests */
    RUN_TEST(test_frobenius_norm_identity);
    RUN_TEST(test_frobenius_norm_ones);
    RUN_TEST(test_frobenius_norm_known);
    RUN_TEST(test_row_sum_norm);

    /* Error recurrence tests */
    RUN_TEST(test_error_contributions);
    RUN_TEST(test_error_recurrence_single_layer);
    RUN_TEST(test_error_recurrence_chain);

    /* Entry error tests */
    RUN_TEST(test_entry_error_q16);
    RUN_TEST(test_entry_error_q24);

    /* Context and total error tests */
    RUN_TEST(test_analysis_ctx_init);
    RUN_TEST(test_compute_total_error);
    RUN_TEST(test_analysis_passed_helper);

    /* Digest tests */
    RUN_TEST(test_digest_generation);
    RUN_TEST(test_digest_null_inputs);

    /* Utility tests */
    RUN_TEST(test_range_magnitude);
    RUN_TEST(test_scale_from_exp);

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
