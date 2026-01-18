/**
 * @file test_verify.c
 * @project Certifiable-Quant
 * @brief Unit tests for verification module
 *
 * @traceability SRS-004-VERIFY TC-VER-01 through TC-VER-08
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#include "verify.h"
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
 * TC-VER-02: L-infinity Norm Tests
 * ============================================================================ */

TEST(test_linf_norm_identical)
{
    float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[] = {1.0f, 2.0f, 3.0f, 4.0f};

    double result = cq_linf_norm(a, b, 4);

    ASSERT_NEAR(result, 0.0, 1e-10, "identical arrays should have zero norm");
    return 1;
}

TEST(test_linf_norm_single_diff)
{
    float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[] = {1.0f, 2.0f, 3.5f, 4.0f};

    double result = cq_linf_norm(a, b, 4);

    ASSERT_NEAR(result, 0.5, 1e-6, "single diff of 0.5 should give 0.5");
    return 1;
}

TEST(test_linf_norm_max_at_end)
{
    float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[] = {1.1f, 2.2f, 3.3f, 6.0f};

    double result = cq_linf_norm(a, b, 4);

    ASSERT_NEAR(result, 2.0, 1e-6, "max diff at end should be found");
    return 1;
}

TEST(test_linf_norm_negative_values)
{
    float a[] = {-1.0f, -2.0f, 3.0f};
    float b[] = {-1.5f, -1.0f, 2.0f};

    double result = cq_linf_norm(a, b, 3);

    /* Diffs: 0.5, 1.0, 1.0 -> max = 1.0 */
    ASSERT_NEAR(result, 1.0, 1e-6, "should handle negative values");
    return 1;
}

TEST(test_linf_norm_null_inputs)
{
    float a[] = {1.0f, 2.0f};

    double result1 = cq_linf_norm(NULL, a, 2);
    double result2 = cq_linf_norm(a, NULL, 2);
    double result3 = cq_linf_norm(a, a, 0);

    ASSERT_NEAR(result1, 0.0, 1e-10, "NULL first array should return 0");
    ASSERT_NEAR(result2, 0.0, 1e-10, "NULL second array should return 0");
    ASSERT_NEAR(result3, 0.0, 1e-10, "zero length should return 0");
    return 1;
}

/* ============================================================================
 * TC-VER-02: L-infinity Norm Q16 Tests
 * ============================================================================ */

TEST(test_linf_norm_q16_exact)
{
    /* 1.0 in Q16.16 = 65536 */
    float fp[] = {1.0f, 2.0f, 0.5f};
    cq_fixed16_t q16[] = {65536, 131072, 32768};

    double result = cq_linf_norm_q16(fp, q16, 3);

    ASSERT_NEAR(result, 0.0, 1e-6, "exact Q16 conversion should have near-zero error");
    return 1;
}

TEST(test_linf_norm_q16_with_error)
{
    float fp[] = {1.0f, 2.0f, 0.5f};
    /* Introduce 0.001 error in second element: 2.001 * 65536 = 131137.536 ≈ 131138 */
    cq_fixed16_t q16[] = {65536, 131138, 32768};

    double result = cq_linf_norm_q16(fp, q16, 3);

    /* Error should be approximately 0.001 + quantization noise */
    ASSERT(result > 0.0, "should detect Q16 error");
    ASSERT(result < 0.01, "error should be small");
    return 1;
}

/* ============================================================================
 * TC-VER-03: Bound Satisfaction Tests
 * ============================================================================ */

TEST(test_bounds_satisfied)
{
    cq_layer_comparison_t layer;
    cq_fault_flags_t faults;

    cq_layer_comparison_init(&layer, 0, 0.01);  /* bound = 0.01 */
    cq_fault_clear(&faults);

    layer.error_max_measured = 0.005;  /* below bound */

    int result = cq_verify_check_bounds(&layer, &faults);

    ASSERT(result == 0, "should return 0 when bound satisfied");
    ASSERT(layer.bound_satisfied == true, "bound_satisfied should be true");
    ASSERT(faults.bound_violation == 0, "no fault should be set");
    return 1;
}

TEST(test_bounds_exactly_equal)
{
    cq_layer_comparison_t layer;
    cq_fault_flags_t faults;

    cq_layer_comparison_init(&layer, 0, 0.01);
    cq_fault_clear(&faults);

    layer.error_max_measured = 0.01;  /* exactly at bound */

    int result = cq_verify_check_bounds(&layer, &faults);

    ASSERT(result == 0, "exactly at bound should pass");
    ASSERT(layer.bound_satisfied == true, "bound_satisfied should be true");
    return 1;
}

TEST(test_bounds_violated)
{
    cq_layer_comparison_t layer;
    cq_fault_flags_t faults;

    cq_layer_comparison_init(&layer, 0, 0.01);
    cq_fault_clear(&faults);

    layer.error_max_measured = 0.015;  /* above bound */

    int result = cq_verify_check_bounds(&layer, &faults);

    ASSERT(result == CQ_FAULT_BOUND_VIOLATION, "should return violation code");
    ASSERT(layer.bound_satisfied == false, "bound_satisfied should be false");
    ASSERT(faults.bound_violation == 1, "fault flag should be set");
    return 1;
}

/* ============================================================================
 * TC-VER-03: Check All Bounds Tests
 * ============================================================================ */

TEST(test_check_all_bounds_all_pass)
{
    cq_layer_comparison_t layers[3];
    cq_verification_report_t report;
    cq_fault_flags_t faults;

    /* Initialize layers with bounds */
    for (int i = 0; i < 3; i++) {
        cq_layer_comparison_init(&layers[i], (uint32_t)i, 0.01);
        layers[i].error_max_measured = 0.005;  /* all below bound */
    }

    cq_verification_report_init(&report, 3, layers, 0.03);
    report.total_error_max_measured = 0.02;  /* below total bound */
    cq_fault_clear(&faults);

    int result = cq_verify_check_all_bounds(&report, &faults);

    ASSERT(result == 0, "all passing should return 0");
    ASSERT(report.all_bounds_satisfied == true, "all_bounds_satisfied should be true");
    ASSERT(report.total_bound_satisfied == true, "total_bound_satisfied should be true");
    return 1;
}

TEST(test_check_all_bounds_one_layer_fails)
{
    cq_layer_comparison_t layers[3];
    cq_verification_report_t report;
    cq_fault_flags_t faults;

    for (int i = 0; i < 3; i++) {
        cq_layer_comparison_init(&layers[i], (uint32_t)i, 0.01);
        layers[i].error_max_measured = 0.005;
    }
    layers[1].error_max_measured = 0.02;  /* middle layer fails */

    cq_verification_report_init(&report, 3, layers, 0.03);
    report.total_error_max_measured = 0.02;
    cq_fault_clear(&faults);

    int result = cq_verify_check_all_bounds(&report, &faults);

    ASSERT(result == CQ_FAULT_BOUND_VIOLATION, "one failing layer should trigger violation");
    ASSERT(report.all_bounds_satisfied == false, "all_bounds_satisfied should be false");
    ASSERT(layers[0].bound_satisfied == true, "layer 0 should pass");
    ASSERT(layers[1].bound_satisfied == false, "layer 1 should fail");
    ASSERT(layers[2].bound_satisfied == true, "layer 2 should pass");
    return 1;
}

TEST(test_check_all_bounds_total_fails)
{
    cq_layer_comparison_t layers[2];
    cq_verification_report_t report;
    cq_fault_flags_t faults;

    for (int i = 0; i < 2; i++) {
        cq_layer_comparison_init(&layers[i], (uint32_t)i, 0.01);
        layers[i].error_max_measured = 0.005;  /* all layers pass */
    }

    cq_verification_report_init(&report, 2, layers, 0.01);
    report.total_error_max_measured = 0.02;  /* total exceeds bound */
    cq_fault_clear(&faults);

    int result = cq_verify_check_all_bounds(&report, &faults);

    ASSERT(result == CQ_FAULT_BOUND_VIOLATION, "total exceeding should trigger violation");
    ASSERT(report.all_bounds_satisfied == true, "all layer bounds should pass");
    ASSERT(report.total_bound_satisfied == false, "total_bound_satisfied should be false");
    return 1;
}

/* ============================================================================
 * TC-VER-05: Statistical Aggregation Tests
 * ============================================================================ */

TEST(test_layer_stats_single_sample)
{
    cq_layer_comparison_t layer;
    cq_layer_comparison_init(&layer, 0, 0.1);

    cq_verify_layer_update(&layer, 0.05);
    cq_verify_layer_finalize(&layer);

    ASSERT(layer.sample_count == 1, "sample count should be 1");
    ASSERT_NEAR(layer.error_max_measured, 0.05, 1e-10, "max should be 0.05");
    ASSERT_NEAR(layer.error_mean_measured, 0.05, 1e-10, "mean should be 0.05");
    ASSERT_NEAR(layer.error_std_measured, 0.0, 1e-10, "std should be 0 for single sample");
    return 1;
}

TEST(test_layer_stats_multiple_samples)
{
    cq_layer_comparison_t layer;
    cq_layer_comparison_init(&layer, 0, 0.1);

    /* Samples: 0.01, 0.02, 0.03, 0.04, 0.05 */
    cq_verify_layer_update(&layer, 0.01);
    cq_verify_layer_update(&layer, 0.02);
    cq_verify_layer_update(&layer, 0.03);
    cq_verify_layer_update(&layer, 0.04);
    cq_verify_layer_update(&layer, 0.05);
    cq_verify_layer_finalize(&layer);

    ASSERT(layer.sample_count == 5, "sample count should be 5");
    ASSERT_NEAR(layer.error_max_measured, 0.05, 1e-10, "max should be 0.05");
    ASSERT_NEAR(layer.error_mean_measured, 0.03, 1e-10, "mean should be 0.03");

    /* Variance = E[X^2] - E[X]^2 = (0.0055/5) - 0.0009 = 0.0011 - 0.0009 = 0.0002 */
    /* Std = sqrt(0.0002) ≈ 0.01414 */
    ASSERT_NEAR(layer.error_std_measured, 0.01414, 0.001, "std should be ~0.014");
    return 1;
}

TEST(test_layer_stats_max_not_last)
{
    cq_layer_comparison_t layer;
    cq_layer_comparison_init(&layer, 0, 0.1);

    cq_verify_layer_update(&layer, 0.01);
    cq_verify_layer_update(&layer, 0.08);  /* max in middle */
    cq_verify_layer_update(&layer, 0.02);
    cq_verify_layer_finalize(&layer);

    ASSERT_NEAR(layer.error_max_measured, 0.08, 1e-10, "max should track correctly");
    return 1;
}

TEST(test_total_stats)
{
    cq_layer_comparison_t layers[1];
    cq_verification_report_t report;

    cq_layer_comparison_init(&layers[0], 0, 0.1);
    cq_verification_report_init(&report, 1, layers, 0.1);

    cq_verify_total_update(&report, 0.02);
    cq_verify_total_update(&report, 0.04);
    cq_verify_total_update(&report, 0.06);
    cq_verify_total_finalize(&report);

    ASSERT(report.sample_count == 3, "sample count should be 3");
    ASSERT_NEAR(report.total_error_max_measured, 0.06, 1e-10, "max should be 0.06");
    ASSERT_NEAR(report.total_error_mean, 0.04, 1e-10, "mean should be 0.04");
    return 1;
}

/* ============================================================================
 * TC-VER-06: Digest Generation Tests
 * ============================================================================ */

TEST(test_digest_generation_pass)
{
    cq_layer_comparison_t layers[2];
    cq_verification_report_t report;
    cq_verification_digest_t digest;

    for (int i = 0; i < 2; i++) {
        cq_layer_comparison_init(&layers[i], (uint32_t)i, 0.01);
        layers[i].error_max_measured = 0.005;
        layers[i].bound_satisfied = true;
    }

    cq_verification_report_init(&report, 2, layers, 0.02);
    report.sample_count = 100;
    report.total_error_max_measured = 0.015;
    report.all_bounds_satisfied = true;
    report.total_bound_satisfied = true;

    /* Set a known hash */
    memset(report.verification_set_hash, 0xAB, 32);

    int result = cq_verification_digest_generate(&report, &digest);

    ASSERT(result == 0, "digest generation should succeed");
    ASSERT(digest.sample_count == 100, "sample count should match");
    ASSERT(digest.layers_passed == 2, "all layers should pass");
    ASSERT_NEAR(digest.total_error_theoretical, 0.02, 1e-10, "theoretical should match");
    ASSERT_NEAR(digest.total_error_max_measured, 0.015, 1e-10, "measured should match");
    ASSERT(digest.bounds_satisfied == 1, "bounds_satisfied should be 1");
    ASSERT(digest.verification_set_hash[0] == 0xAB, "hash should be copied");
    return 1;
}

TEST(test_digest_generation_fail)
{
    cq_layer_comparison_t layers[2];
    cq_verification_report_t report;
    cq_verification_digest_t digest;

    cq_layer_comparison_init(&layers[0], 0, 0.01);
    layers[0].error_max_measured = 0.005;
    layers[0].bound_satisfied = true;

    cq_layer_comparison_init(&layers[1], 1, 0.01);
    layers[1].error_max_measured = 0.02;
    layers[1].bound_satisfied = false;  /* one layer fails */

    cq_verification_report_init(&report, 2, layers, 0.02);
    report.sample_count = 100;
    report.total_error_max_measured = 0.02;
    report.all_bounds_satisfied = false;
    report.total_bound_satisfied = true;

    int result = cq_verification_digest_generate(&report, &digest);

    ASSERT(result == 0, "digest generation should still succeed");
    ASSERT(digest.layers_passed == 1, "only one layer should pass");
    ASSERT(digest.bounds_satisfied == 0, "bounds_satisfied should be 0");
    return 1;
}

TEST(test_digest_null_inputs)
{
    cq_verification_report_t report;
    cq_verification_digest_t digest;

    int result1 = cq_verification_digest_generate(NULL, &digest);
    int result2 = cq_verification_digest_generate(&report, NULL);

    ASSERT(result1 == CQ_ERROR_NULL_POINTER, "NULL report should error");
    ASSERT(result2 == CQ_ERROR_NULL_POINTER, "NULL digest should error");
    return 1;
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST(test_q16_to_float)
{
    ASSERT_NEAR(cq_q16_to_float(65536), 1.0f, 1e-6, "65536 should be 1.0");
    ASSERT_NEAR(cq_q16_to_float(131072), 2.0f, 1e-6, "131072 should be 2.0");
    ASSERT_NEAR(cq_q16_to_float(32768), 0.5f, 1e-6, "32768 should be 0.5");
    ASSERT_NEAR(cq_q16_to_float(-65536), -1.0f, 1e-6, "-65536 should be -1.0");
    ASSERT_NEAR(cq_q16_to_float(0), 0.0f, 1e-10, "0 should be 0.0");
    return 1;
}

TEST(test_verify_passed_helper)
{
    cq_layer_comparison_t layers[1];
    cq_verification_report_t report;

    cq_layer_comparison_init(&layers[0], 0, 0.1);
    cq_verification_report_init(&report, 1, layers, 0.1);

    report.all_bounds_satisfied = true;
    report.total_bound_satisfied = true;
    ASSERT(cq_verify_passed(&report) == true, "should pass when both true");

    report.all_bounds_satisfied = false;
    report.total_bound_satisfied = true;
    ASSERT(cq_verify_passed(&report) == false, "should fail when layers fail");

    report.all_bounds_satisfied = true;
    report.total_bound_satisfied = false;
    ASSERT(cq_verify_passed(&report) == false, "should fail when total fails");

    return 1;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    printf("=== Certifiable-Quant Verify Module Tests ===\n\n");

    /* L-infinity norm tests */
    RUN_TEST(test_linf_norm_identical);
    RUN_TEST(test_linf_norm_single_diff);
    RUN_TEST(test_linf_norm_max_at_end);
    RUN_TEST(test_linf_norm_negative_values);
    RUN_TEST(test_linf_norm_null_inputs);

    /* L-infinity Q16 tests */
    RUN_TEST(test_linf_norm_q16_exact);
    RUN_TEST(test_linf_norm_q16_with_error);

    /* Bound satisfaction tests */
    RUN_TEST(test_bounds_satisfied);
    RUN_TEST(test_bounds_exactly_equal);
    RUN_TEST(test_bounds_violated);

    /* Check all bounds tests */
    RUN_TEST(test_check_all_bounds_all_pass);
    RUN_TEST(test_check_all_bounds_one_layer_fails);
    RUN_TEST(test_check_all_bounds_total_fails);

    /* Statistical aggregation tests */
    RUN_TEST(test_layer_stats_single_sample);
    RUN_TEST(test_layer_stats_multiple_samples);
    RUN_TEST(test_layer_stats_max_not_last);
    RUN_TEST(test_total_stats);

    /* Digest generation tests */
    RUN_TEST(test_digest_generation_pass);
    RUN_TEST(test_digest_generation_fail);
    RUN_TEST(test_digest_null_inputs);

    /* Utility tests */
    RUN_TEST(test_q16_to_float);
    RUN_TEST(test_verify_passed_helper);

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
