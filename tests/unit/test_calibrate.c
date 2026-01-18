/**
 * @file test_calibrate.c
 * @project Certifiable-Quant
 * @brief Unit tests for calibration module
 *
 * @traceability SRS-002-CALIBRATE TC-CAL-01 through TC-CAL-07
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#include "calibrate.h"
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
    if (fabsf((float)(a) - (float)(b)) > (tol)) { \
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
 * TC-CAL-01: Statistics Collection Tests
 * ============================================================================ */

TEST(test_tensor_stats_init)
{
    cq_tensor_stats_t stats;

    cq_tensor_stats_init(&stats, 42, 3, -1.0f, 1.0f);

    ASSERT(stats.tensor_id == 42, "tensor_id should be 42");
    ASSERT(stats.layer_index == 3, "layer_index should be 3");
    ASSERT_NEAR(stats.min_safe, -1.0f, 1e-6f, "min_safe should be -1.0");
    ASSERT_NEAR(stats.max_safe, 1.0f, 1e-6f, "max_safe should be 1.0");
    ASSERT(stats.min_observed == FLT_MAX, "min_observed should be FLT_MAX");
    ASSERT(stats.max_observed == -FLT_MAX, "max_observed should be -FLT_MAX");
    ASSERT(stats.is_degenerate == false, "should not be degenerate");
    ASSERT(stats.range_veto == false, "should not have range veto");
    return 1;
}

TEST(test_tensor_stats_update_basic)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -10.0f, 10.0f);

    float tensor[] = {1.0f, -2.0f, 3.0f, -4.0f, 5.0f};
    cq_tensor_stats_update(&stats, tensor, 5);

    ASSERT_NEAR(stats.min_observed, -4.0f, 1e-6f, "min should be -4.0");
    ASSERT_NEAR(stats.max_observed, 5.0f, 1e-6f, "max should be 5.0");
    return 1;
}

TEST(test_tensor_stats_update_multiple)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -10.0f, 10.0f);

    /* First batch */
    float batch1[] = {1.0f, 2.0f, 3.0f};
    cq_tensor_stats_update(&stats, batch1, 3);

    ASSERT_NEAR(stats.min_observed, 1.0f, 1e-6f, "min after batch1");
    ASSERT_NEAR(stats.max_observed, 3.0f, 1e-6f, "max after batch1");

    /* Second batch extends range */
    float batch2[] = {-5.0f, 0.0f, 7.0f};
    cq_tensor_stats_update(&stats, batch2, 3);

    ASSERT_NEAR(stats.min_observed, -5.0f, 1e-6f, "min after batch2");
    ASSERT_NEAR(stats.max_observed, 7.0f, 1e-6f, "max after batch2");
    return 1;
}

TEST(test_tensor_stats_update_single)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -10.0f, 10.0f);

    cq_tensor_stats_update_single(&stats, 5.0f);
    ASSERT_NEAR(stats.min_observed, 5.0f, 1e-6f, "min after first");
    ASSERT_NEAR(stats.max_observed, 5.0f, 1e-6f, "max after first");

    cq_tensor_stats_update_single(&stats, -3.0f);
    ASSERT_NEAR(stats.min_observed, -3.0f, 1e-6f, "min after second");
    ASSERT_NEAR(stats.max_observed, 5.0f, 1e-6f, "max after second");

    cq_tensor_stats_update_single(&stats, 8.0f);
    ASSERT_NEAR(stats.min_observed, -3.0f, 1e-6f, "min after third");
    ASSERT_NEAR(stats.max_observed, 8.0f, 1e-6f, "max after third");
    return 1;
}

TEST(test_tensor_stats_skip_nan_inf)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -10.0f, 10.0f);

    float tensor[] = {1.0f, NAN, 3.0f, INFINITY, -2.0f, -INFINITY};
    cq_tensor_stats_update(&stats, tensor, 6);

    /* Should only consider 1.0, 3.0, -2.0 */
    ASSERT_NEAR(stats.min_observed, -2.0f, 1e-6f, "min should skip NaN/Inf");
    ASSERT_NEAR(stats.max_observed, 3.0f, 1e-6f, "max should skip NaN/Inf");
    return 1;
}

/* ============================================================================
 * TC-CAL-02: Coverage Computation Tests
 * ============================================================================ */

TEST(test_coverage_perfect)
{
    cq_tensor_stats_t stats;
    cq_calibrate_config_t config = CQ_CALIBRATE_CONFIG_DEFAULT;

    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = -1.0f;
    stats.max_observed = 1.0f;

    cq_tensor_compute_coverage(&stats, &config);

    /* Coverage = (1 - (-1)) / (1 - (-1)) = 2/2 = 1.0 */
    ASSERT_NEAR(stats.coverage_ratio, 1.0f, 1e-6f, "perfect coverage should be 1.0");
    ASSERT(stats.is_degenerate == false, "should not be degenerate");
    return 1;
}

TEST(test_coverage_partial)
{
    cq_tensor_stats_t stats;
    cq_calibrate_config_t config = CQ_CALIBRATE_CONFIG_DEFAULT;

    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = -0.5f;
    stats.max_observed = 0.5f;

    cq_tensor_compute_coverage(&stats, &config);

    /* Coverage = (0.5 - (-0.5)) / (1 - (-1)) = 1/2 = 0.5 */
    ASSERT_NEAR(stats.coverage_ratio, 0.5f, 1e-6f, "partial coverage should be 0.5");
    return 1;
}

TEST(test_coverage_asymmetric)
{
    cq_tensor_stats_t stats;
    cq_calibrate_config_t config = CQ_CALIBRATE_CONFIG_DEFAULT;

    cq_tensor_stats_init(&stats, 0, 0, 0.0f, 10.0f);
    stats.min_observed = 2.0f;
    stats.max_observed = 8.0f;

    cq_tensor_compute_coverage(&stats, &config);

    /* Coverage = (8 - 2) / (10 - 0) = 6/10 = 0.6 */
    ASSERT_NEAR(stats.coverage_ratio, 0.6f, 1e-6f, "asymmetric coverage");
    return 1;
}

/* ============================================================================
 * TC-CAL-03: Range Veto Tests
 * ============================================================================ */

TEST(test_range_veto_pass)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = -0.5f;
    stats.max_observed = 0.5f;

    bool veto = cq_tensor_check_range_veto(&stats);

    ASSERT(veto == false, "should not trigger veto");
    ASSERT(stats.range_veto == false, "range_veto should be false");
    ASSERT(cq_tensor_range_valid(&stats) == true, "should be valid");
    return 1;
}

TEST(test_range_veto_exact_boundary)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = -1.0f;
    stats.max_observed = 1.0f;

    bool veto = cq_tensor_check_range_veto(&stats);

    ASSERT(veto == false, "exact boundary should not trigger veto");
    ASSERT(stats.range_veto == false, "range_veto should be false");
    return 1;
}

TEST(test_range_veto_exceed_max)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = 0.0f;
    stats.max_observed = 1.5f;  /* Exceeds max_safe */

    bool veto = cq_tensor_check_range_veto(&stats);

    ASSERT(veto == true, "exceeding max should trigger veto");
    ASSERT(stats.range_veto == true, "range_veto should be true");
    ASSERT(cq_tensor_range_valid(&stats) == false, "should be invalid");
    return 1;
}

TEST(test_range_veto_exceed_min)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = -1.5f;  /* Exceeds min_safe */
    stats.max_observed = 0.5f;

    bool veto = cq_tensor_check_range_veto(&stats);

    ASSERT(veto == true, "exceeding min should trigger veto");
    ASSERT(stats.range_veto == true, "range_veto should be true");
    return 1;
}

TEST(test_range_veto_exceed_both)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = -2.0f;
    stats.max_observed = 2.0f;

    bool veto = cq_tensor_check_range_veto(&stats);

    ASSERT(veto == true, "exceeding both should trigger veto");
    return 1;
}

/* ============================================================================
 * TC-CAL-05: Degenerate Tensor Tests
 * ============================================================================ */

TEST(test_degenerate_zero_range)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = 0.5f;
    stats.max_observed = 0.5f;  /* Same value */

    bool is_deg = cq_tensor_check_degenerate(&stats, 1e-7f);

    ASSERT(is_deg == true, "zero range should be degenerate");
    ASSERT(stats.is_degenerate == true, "is_degenerate should be true");
    return 1;
}

TEST(test_degenerate_tiny_range)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = 0.5f;
    stats.max_observed = 0.5f + 1e-8f;  /* Tiny range */

    bool is_deg = cq_tensor_check_degenerate(&stats, 1e-7f);

    ASSERT(is_deg == true, "tiny range should be degenerate");
    return 1;
}

TEST(test_degenerate_normal_range)
{
    cq_tensor_stats_t stats;
    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = 0.0f;
    stats.max_observed = 0.5f;

    bool is_deg = cq_tensor_check_degenerate(&stats, 1e-7f);

    ASSERT(is_deg == false, "normal range should not be degenerate");
    ASSERT(stats.is_degenerate == false, "is_degenerate should be false");
    return 1;
}

TEST(test_coverage_degenerate_handling)
{
    cq_tensor_stats_t stats;
    cq_calibrate_config_t config = CQ_CALIBRATE_CONFIG_DEFAULT;

    cq_tensor_stats_init(&stats, 0, 0, -1.0f, 1.0f);
    stats.min_observed = 0.5f;
    stats.max_observed = 0.5f;  /* Degenerate */

    cq_tensor_compute_coverage(&stats, &config);

    ASSERT(stats.is_degenerate == true, "should be marked degenerate");
    ASSERT_NEAR(stats.coverage_ratio, 1.0f, 1e-6f, "degenerate coverage should be 1.0");
    return 1;
}

/* ============================================================================
 * TC-CAL-04: Global Coverage Tests
 * ============================================================================ */

TEST(test_global_coverage_uniform)
{
    cq_tensor_stats_t tensors[5];
    cq_calibration_report_t report;

    /* All tensors have same coverage */
    for (int i = 0; i < 5; i++) {
        cq_tensor_stats_init(&tensors[i], (uint32_t)i, 0, -1.0f, 1.0f);
        tensors[i].coverage_ratio = 0.8f;
    }

    cq_calibration_report_init(&report, 5, tensors);
    cq_calibration_compute_global_coverage(&report);

    ASSERT_NEAR(report.global_coverage_min, 0.8f, 1e-6f, "min should be 0.8");
    ASSERT_NEAR(report.global_coverage_mean, 0.8f, 1e-6f, "mean should be 0.8");
    ASSERT_NEAR(report.global_coverage_p10, 0.8f, 1e-6f, "p10 should be 0.8");
    return 1;
}

TEST(test_global_coverage_varied)
{
    cq_tensor_stats_t tensors[10];
    cq_calibration_report_t report;

    /* Coverage values: 0.5, 0.6, 0.7, 0.8, 0.9, 0.9, 0.9, 0.95, 0.95, 1.0 */
    const float coverages[] = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 0.9f, 0.9f, 0.95f, 0.95f, 1.0f};

    for (int i = 0; i < 10; i++) {
        cq_tensor_stats_init(&tensors[i], (uint32_t)i, 0, -1.0f, 1.0f);
        tensors[i].coverage_ratio = coverages[i];
    }

    cq_calibration_report_init(&report, 10, tensors);
    cq_calibration_compute_global_coverage(&report);

    ASSERT_NEAR(report.global_coverage_min, 0.5f, 1e-6f, "min should be 0.5");

    /* Mean = (0.5+0.6+0.7+0.8+0.9+0.9+0.9+0.95+0.95+1.0)/10 = 8.2/10 = 0.82 */
    ASSERT_NEAR(report.global_coverage_mean, 0.82f, 1e-6f, "mean should be 0.82");

    /* P10 index = 0.1 * 10 = 1, sorted[1] = 0.6 */
    ASSERT_NEAR(report.global_coverage_p10, 0.6f, 1e-6f, "p10 should be 0.6");
    return 1;
}

TEST(test_coverage_threshold_pass)
{
    cq_tensor_stats_t tensors[3];
    cq_calibration_report_t report;
    cq_calibrate_config_t config = CQ_CALIBRATE_CONFIG_DEFAULT;

    for (int i = 0; i < 3; i++) {
        cq_tensor_stats_init(&tensors[i], (uint32_t)i, 0, -1.0f, 1.0f);
        tensors[i].coverage_ratio = 0.95f;  /* Above thresholds */
    }

    cq_calibration_report_init(&report, 3, tensors);
    cq_calibration_compute_global_coverage(&report);

    bool veto = cq_calibration_check_coverage_threshold(&report, &config);

    ASSERT(veto == false, "good coverage should not trigger veto");
    return 1;
}

TEST(test_coverage_threshold_fail_min)
{
    cq_tensor_stats_t tensors[3];
    cq_calibration_report_t report;
    cq_calibrate_config_t config = CQ_CALIBRATE_CONFIG_DEFAULT;

    tensors[0].coverage_ratio = 0.5f;   /* Below C_min threshold */
    tensors[1].coverage_ratio = 0.95f;
    tensors[2].coverage_ratio = 0.95f;

    cq_calibration_report_init(&report, 3, tensors);
    cq_calibration_compute_global_coverage(&report);

    bool veto = cq_calibration_check_coverage_threshold(&report, &config);

    ASSERT(veto == true, "low C_min should trigger veto");
    return 1;
}

/* ============================================================================
 * Report Finalization Tests
 * ============================================================================ */

TEST(test_report_finalize_pass)
{
    cq_tensor_stats_t tensors[2];
    cq_calibration_report_t report;
    cq_calibrate_config_t config = CQ_CALIBRATE_CONFIG_DEFAULT;
    cq_fault_flags_t faults;

    /* Set up tensors with valid ranges */
    cq_tensor_stats_init(&tensors[0], 0, 0, -1.0f, 1.0f);
    tensors[0].min_observed = -0.8f;
    tensors[0].max_observed = 0.8f;

    cq_tensor_stats_init(&tensors[1], 1, 1, 0.0f, 10.0f);
    tensors[1].min_observed = 1.0f;
    tensors[1].max_observed = 9.0f;

    cq_calibration_report_init(&report, 2, tensors);
    report.sample_count = 100;
    cq_fault_clear(&faults);

    int result = cq_calibration_report_finalize(&report, &config, &faults);

    ASSERT(result == 0, "finalize should succeed");
    ASSERT(report.range_veto_triggered == false, "no range veto");
    ASSERT(faults.range_exceed == 0, "no range fault");
    ASSERT(cq_calibration_passed(&report) == true, "should pass");
    return 1;
}

TEST(test_report_finalize_range_veto)
{
    cq_tensor_stats_t tensors[2];
    cq_calibration_report_t report;
    cq_calibrate_config_t config = CQ_CALIBRATE_CONFIG_DEFAULT;
    cq_fault_flags_t faults;

    /* First tensor valid */
    cq_tensor_stats_init(&tensors[0], 0, 0, -1.0f, 1.0f);
    tensors[0].min_observed = -0.5f;
    tensors[0].max_observed = 0.5f;

    /* Second tensor exceeds range */
    cq_tensor_stats_init(&tensors[1], 1, 1, 0.0f, 1.0f);
    tensors[1].min_observed = 0.0f;
    tensors[1].max_observed = 2.0f;  /* Exceeds! */

    cq_calibration_report_init(&report, 2, tensors);
    cq_fault_clear(&faults);

    int result = cq_calibration_report_finalize(&report, &config, &faults);

    ASSERT(result == 0, "finalize should succeed");
    ASSERT(report.range_veto_triggered == true, "range veto should trigger");
    ASSERT(faults.range_exceed == 1, "range fault should be set");
    ASSERT(cq_calibration_passed(&report) == false, "should not pass");
    return 1;
}

/* ============================================================================
 * TC-CAL-06: Digest Generation Tests
 * ============================================================================ */

TEST(test_digest_generation)
{
    cq_tensor_stats_t tensors[2];
    cq_calibration_report_t report;
    cq_calibration_digest_t digest;

    cq_tensor_stats_init(&tensors[0], 0, 0, -1.0f, 1.0f);
    cq_tensor_stats_init(&tensors[1], 1, 0, -1.0f, 1.0f);
    tensors[0].coverage_ratio = 0.9f;
    tensors[1].coverage_ratio = 0.95f;

    cq_calibration_report_init(&report, 2, tensors);
    report.sample_count = 500;
    report.global_coverage_min = 0.9f;
    report.global_coverage_p10 = 0.92f;
    report.range_veto_triggered = false;
    report.coverage_veto_triggered = false;

    /* Set a known hash */
    memset(report.dataset_hash, 0xCD, 32);

    int result = cq_calibration_digest_generate(&report, &digest);

    ASSERT(result == 0, "digest generation should succeed");
    ASSERT(digest.sample_count == 500, "sample count should match");
    ASSERT(digest.tensor_count == 2, "tensor count should match");
    ASSERT_NEAR(digest.global_coverage_min, 0.9f, 1e-6f, "coverage min should match");
    ASSERT_NEAR(digest.global_coverage_p10, 0.92f, 1e-6f, "coverage p10 should match");
    ASSERT(digest.range_veto_status == 0, "range veto should be 0");
    ASSERT(digest.coverage_veto_status == 0, "coverage veto should be 0");
    ASSERT(digest.dataset_hash[0] == 0xCD, "hash should be copied");
    return 1;
}

TEST(test_digest_with_veto)
{
    cq_calibration_report_t report;
    cq_calibration_digest_t digest;

    cq_calibration_report_init(&report, 0, NULL);
    report.range_veto_triggered = true;
    report.coverage_veto_triggered = true;

    int result = cq_calibration_digest_generate(&report, &digest);

    ASSERT(result == 0, "digest generation should succeed");
    ASSERT(digest.range_veto_status == 1, "range veto should be 1");
    ASSERT(digest.coverage_veto_status == 1, "coverage veto should be 1");
    return 1;
}

TEST(test_digest_null_inputs)
{
    cq_calibration_report_t report;
    cq_calibration_digest_t digest;

    int result1 = cq_calibration_digest_generate(NULL, &digest);
    int result2 = cq_calibration_digest_generate(&report, NULL);

    ASSERT(result1 == CQ_ERROR_NULL_POINTER, "NULL report should error");
    ASSERT(result2 == CQ_ERROR_NULL_POINTER, "NULL digest should error");
    return 1;
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST(test_passed_helpers)
{
    cq_calibration_report_t report;

    cq_calibration_report_init(&report, 0, NULL);

    report.range_veto_triggered = false;
    report.coverage_veto_triggered = false;
    ASSERT(cq_calibration_passed(&report) == true, "no veto = pass");
    ASSERT(cq_calibration_passed_full(&report) == true, "no veto = full pass");

    report.range_veto_triggered = true;
    ASSERT(cq_calibration_passed(&report) == false, "range veto = fail");
    ASSERT(cq_calibration_passed_full(&report) == false, "range veto = full fail");

    report.range_veto_triggered = false;
    report.coverage_veto_triggered = true;
    ASSERT(cq_calibration_passed(&report) == true, "coverage veto = pass (warning only)");
    ASSERT(cq_calibration_passed_full(&report) == false, "coverage veto = not full pass");

    return 1;
}

TEST(test_add_sample)
{
    cq_calibration_report_t report;
    cq_calibration_report_init(&report, 0, NULL);

    ASSERT(report.sample_count == 0, "initial sample count should be 0");

    cq_calibration_report_add_sample(&report);
    ASSERT(report.sample_count == 1, "sample count should be 1");

    cq_calibration_report_add_sample(&report);
    cq_calibration_report_add_sample(&report);
    ASSERT(report.sample_count == 3, "sample count should be 3");

    return 1;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    printf("=== Certifiable-Quant Calibrate Module Tests ===\n\n");

    /* Statistics collection tests */
    RUN_TEST(test_tensor_stats_init);
    RUN_TEST(test_tensor_stats_update_basic);
    RUN_TEST(test_tensor_stats_update_multiple);
    RUN_TEST(test_tensor_stats_update_single);
    RUN_TEST(test_tensor_stats_skip_nan_inf);

    /* Coverage computation tests */
    RUN_TEST(test_coverage_perfect);
    RUN_TEST(test_coverage_partial);
    RUN_TEST(test_coverage_asymmetric);

    /* Range veto tests */
    RUN_TEST(test_range_veto_pass);
    RUN_TEST(test_range_veto_exact_boundary);
    RUN_TEST(test_range_veto_exceed_max);
    RUN_TEST(test_range_veto_exceed_min);
    RUN_TEST(test_range_veto_exceed_both);

    /* Degenerate tensor tests */
    RUN_TEST(test_degenerate_zero_range);
    RUN_TEST(test_degenerate_tiny_range);
    RUN_TEST(test_degenerate_normal_range);
    RUN_TEST(test_coverage_degenerate_handling);

    /* Global coverage tests */
    RUN_TEST(test_global_coverage_uniform);
    RUN_TEST(test_global_coverage_varied);
    RUN_TEST(test_coverage_threshold_pass);
    RUN_TEST(test_coverage_threshold_fail_min);

    /* Report finalization tests */
    RUN_TEST(test_report_finalize_pass);
    RUN_TEST(test_report_finalize_range_veto);

    /* Digest generation tests */
    RUN_TEST(test_digest_generation);
    RUN_TEST(test_digest_with_veto);
    RUN_TEST(test_digest_null_inputs);

    /* Utility tests */
    RUN_TEST(test_passed_helpers);
    RUN_TEST(test_add_sample);

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
