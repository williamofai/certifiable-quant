/**
 * @file test_certificate.c
 * @project Certifiable-Quant
 * @brief Unit tests for certificate module
 *
 * @traceability SRS-005-CERTIFICATE
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#include "certificate.h"
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
 * Helper: Create complete builder
 * ============================================================================ */

static void setup_complete_builder(cq_certificate_builder_t *builder)
{
    cq_certificate_builder_init(builder);

    /* Set version */
    cq_certificate_builder_set_version(builder, 1, 0, 0, 0);

    /* Set source hash */
    uint8_t source_hash[32];
    memset(source_hash, 0xAA, 32);
    cq_certificate_builder_set_source_hash(builder, source_hash);

    /* Set BN info */
    uint8_t bn_hash[32];
    memset(bn_hash, 0xBB, 32);
    cq_certificate_builder_set_bn_info(builder, true, bn_hash);

    /* Set analysis digest */
    cq_analysis_digest_t analysis = {0};
    analysis.entry_error = 7.63e-6;
    analysis.total_error_bound = 1.0e-4;
    analysis.layer_count = 5;
    analysis.overflow_safe_count = 5;
    memset(analysis.layers_hash, 0xCC, 32);
    cq_certificate_builder_set_analysis(builder, &analysis);

    /* Set calibration digest */
    cq_calibration_digest_t calibration = {0};
    calibration.sample_count = 1000;
    calibration.tensor_count = 10;
    calibration.global_coverage_min = 0.92f;
    calibration.global_coverage_p10 = 0.95f;
    calibration.range_veto_status = 0;
    calibration.coverage_veto_status = 0;
    memset(calibration.dataset_hash, 0xDD, 32);
    cq_certificate_builder_set_calibration(builder, &calibration);

    /* Set verification digest */
    cq_verification_digest_t verification = {0};
    verification.sample_count = 500;
    verification.layers_passed = 5;
    verification.total_error_theoretical = 1.0e-4;
    verification.total_error_max_measured = 8.5e-5;
    verification.bounds_satisfied = 1;
    memset(verification.verification_set_hash, 0xEE, 32);
    cq_certificate_builder_set_verification(builder, &verification);

    /* Set target */
    uint8_t target_hash[32];
    memset(target_hash, 0xFF, 32);
    cq_certificate_builder_set_target(builder, target_hash, 100000, 5);
}

/* ============================================================================
 * Builder Tests
 * ============================================================================ */

TEST(test_builder_init)
{
    cq_certificate_builder_t builder;

    cq_certificate_builder_init(&builder);

    ASSERT(builder.source_model_hash_set == false, "source not set initially");
    ASSERT(builder.bn_info_set == false, "bn info not set initially");
    ASSERT(builder.analysis_set == false, "analysis not set initially");
    ASSERT(builder.calibration_set == false, "calibration not set initially");
    ASSERT(builder.verification_set == false, "verification not set initially");
    ASSERT(builder.target_set == false, "target not set initially");
    ASSERT(builder.scope_format == CQ_FORMAT_Q16_16_CODE, "default format should be Q16.16");
    return 1;
}

TEST(test_builder_set_version)
{
    cq_certificate_builder_t builder;
    cq_certificate_builder_init(&builder);

    cq_certificate_builder_set_version(&builder, 1, 2, 3, 4);

    ASSERT(builder.tool_version[0] == 1, "major version");
    ASSERT(builder.tool_version[1] == 2, "minor version");
    ASSERT(builder.tool_version[2] == 3, "patch version");
    ASSERT(builder.tool_version[3] == 4, "build version");
    return 1;
}

TEST(test_builder_set_source_hash)
{
    cq_certificate_builder_t builder;
    cq_certificate_builder_init(&builder);

    uint8_t hash[32];
    memset(hash, 0x42, 32);

    cq_certificate_builder_set_source_hash(&builder, hash);

    ASSERT(builder.source_model_hash_set == true, "source should be set");
    ASSERT(builder.source_model_hash[0] == 0x42, "hash should be copied");
    ASSERT(builder.source_model_hash[31] == 0x42, "hash should be fully copied");
    return 1;
}

TEST(test_builder_set_bn_info)
{
    cq_certificate_builder_t builder;
    cq_certificate_builder_init(&builder);

    uint8_t hash[32];
    memset(hash, 0x55, 32);

    cq_certificate_builder_set_bn_info(&builder, true, hash);

    ASSERT(builder.bn_info_set == true, "bn info should be set");
    ASSERT(builder.bn_folded == true, "bn_folded should be true");
    ASSERT(builder.bn_folding_hash[0] == 0x55, "hash should be copied");
    return 1;
}

TEST(test_builder_set_bn_info_no_bn)
{
    cq_certificate_builder_t builder;
    cq_certificate_builder_init(&builder);

    cq_certificate_builder_set_bn_info(&builder, false, NULL);

    ASSERT(builder.bn_info_set == true, "bn info should be set");
    ASSERT(builder.bn_folded == false, "bn_folded should be false");
    ASSERT(builder.bn_folding_hash[0] == 0, "hash should be zero");
    return 1;
}

TEST(test_builder_is_complete_empty)
{
    cq_certificate_builder_t builder;
    cq_certificate_builder_init(&builder);

    ASSERT(cq_certificate_builder_is_complete(&builder) == false, "empty builder not complete");
    return 1;
}

TEST(test_builder_is_complete_partial)
{
    cq_certificate_builder_t builder;
    cq_certificate_builder_init(&builder);

    uint8_t hash[32] = {0};
    cq_certificate_builder_set_source_hash(&builder, hash);
    cq_certificate_builder_set_bn_info(&builder, false, NULL);

    ASSERT(cq_certificate_builder_is_complete(&builder) == false, "partial builder not complete");
    return 1;
}

TEST(test_builder_is_complete_full)
{
    cq_certificate_builder_t builder;
    setup_complete_builder(&builder);

    ASSERT(cq_certificate_builder_is_complete(&builder) == true, "full builder should be complete");
    return 1;
}

/* ============================================================================
 * Certificate Build Tests
 * ============================================================================ */

TEST(test_certificate_build_success)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);

    int result = cq_certificate_build(&builder, &cert, &faults);

    ASSERT(result == 0, "build should succeed");
    ASSERT(memcmp(cert.magic, "CQCR", 4) == 0, "magic should be CQCR");
    ASSERT(cert.scope_symmetric_only == CQ_SCOPE_SYMMETRIC_ONLY, "scope should be symmetric");
    ASSERT(cert.scope_format == CQ_FORMAT_Q16_16_CODE, "format should be Q16.16");
    ASSERT(cert.bn_folding_status == 0x01, "BN should be folded");
    ASSERT(cert.target_layer_count == 5, "layer count should be 5");
    ASSERT(cert.target_param_count == 100000, "param count should be 100000");
    return 1;
}

TEST(test_certificate_build_claims)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);

    cq_certificate_build(&builder, &cert, &faults);

    ASSERT_NEAR(cert.epsilon_0_claimed, 7.63e-6, 1e-8, "entry error");
    ASSERT_NEAR(cert.epsilon_total_claimed, 1.0e-4, 1e-8, "total error");
    ASSERT_NEAR(cert.epsilon_max_measured, 8.5e-5, 1e-8, "measured error");
    ASSERT(cq_certificate_bounds_satisfied(&cert) == true, "bounds should be satisfied");
    return 1;
}

TEST(test_certificate_build_incomplete)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert;
    cq_fault_flags_t faults;

    cq_certificate_builder_init(&builder);
    cq_fault_clear(&faults);

    int result = cq_certificate_build(&builder, &cert, &faults);

    ASSERT(result != 0, "build should fail for incomplete builder");
    return 1;
}

TEST(test_certificate_build_null_inputs)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);

    int r1 = cq_certificate_build(NULL, &cert, &faults);
    int r2 = cq_certificate_build(&builder, NULL, &faults);
    int r3 = cq_certificate_build(&builder, &cert, NULL);

    ASSERT(r1 == CQ_ERROR_NULL_POINTER, "NULL builder should error");
    ASSERT(r2 == CQ_ERROR_NULL_POINTER, "NULL cert should error");
    ASSERT(r3 == CQ_ERROR_NULL_POINTER, "NULL faults should error");
    return 1;
}

/* ============================================================================
 * Merkle Root Tests
 * ============================================================================ */

TEST(test_merkle_root_computed)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);
    cq_certificate_build(&builder, &cert, &faults);

    /* Merkle root should be non-zero */
    int nonzero = 0;
    for (int i = 0; i < 32; i++) {
        if (cert.merkle_root[i] != 0) {
            nonzero = 1;
            break;
        }
    }
    ASSERT(nonzero, "merkle root should be non-zero");
    return 1;
}

TEST(test_merkle_root_deterministic)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert1, cert2;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);

    cq_certificate_build(&builder, &cert1, &faults);

    /* Force same timestamp for determinism test */
    cert2 = cert1;

    uint8_t root1[32], root2[32];
    cq_certificate_compute_merkle(&cert1, root1);
    cq_certificate_compute_merkle(&cert2, root2);

    ASSERT(memcmp(root1, root2, 32) == 0, "merkle root should be deterministic");
    return 1;
}

TEST(test_merkle_root_changes_with_content)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert1, cert2;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);
    cq_certificate_build(&builder, &cert1, &faults);

    /* Change something */
    cert2 = cert1;
    cert2.target_param_count = 999999;

    uint8_t root1[32], root2[32];
    cq_certificate_compute_merkle(&cert1, root1);
    cq_certificate_compute_merkle(&cert2, root2);

    ASSERT(memcmp(root1, root2, 32) != 0, "merkle root should change with content");
    return 1;
}

/* ============================================================================
 * Integrity Verification Tests
 * ============================================================================ */

TEST(test_verify_integrity_valid)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);
    cq_certificate_build(&builder, &cert, &faults);

    ASSERT(cq_certificate_verify_integrity(&cert) == true, "valid cert should verify");
    return 1;
}

TEST(test_verify_integrity_tampered)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);
    cq_certificate_build(&builder, &cert, &faults);

    /* Tamper with content */
    cert.target_param_count = 1;

    ASSERT(cq_certificate_verify_integrity(&cert) == false, "tampered cert should fail");
    return 1;
}

TEST(test_verify_header_valid)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);
    cq_certificate_build(&builder, &cert, &faults);

    ASSERT(cq_certificate_verify_header(&cert) == true, "valid header should verify");
    return 1;
}

TEST(test_verify_header_bad_magic)
{
    cq_certificate_t cert;
    memset(&cert, 0, sizeof(cert));
    memcpy(cert.magic, "XXXX", 4);
    cert.scope_symmetric_only = CQ_SCOPE_SYMMETRIC_ONLY;
    cert.scope_format = CQ_FORMAT_Q16_16_CODE;

    ASSERT(cq_certificate_verify_header(&cert) == false, "bad magic should fail");
    return 1;
}

/* ============================================================================
 * Serialisation Tests
 * ============================================================================ */

TEST(test_serialise_deserialise_roundtrip)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert_orig, cert_restored;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);
    cq_certificate_build(&builder, &cert_orig, &faults);

    /* Serialise */
    uint8_t buffer[CQ_CERTIFICATE_SIZE];
    size_t size;
    int r1 = cq_certificate_serialise(&cert_orig, buffer, &size);

    ASSERT(r1 == 0, "serialise should succeed");
    ASSERT(size == sizeof(cq_certificate_t), "size should match struct size");

    /* Deserialise */
    int r2 = cq_certificate_deserialise(buffer, size, &cert_restored);

    ASSERT(r2 == 0, "deserialise should succeed");
    ASSERT(memcmp(&cert_orig, &cert_restored, sizeof(cq_certificate_t)) == 0,
           "roundtrip should preserve certificate");
    return 1;
}

TEST(test_deserialise_too_small)
{
    uint8_t buffer[10] = {0};
    cq_certificate_t cert;

    int result = cq_certificate_deserialise(buffer, 10, &cert);

    ASSERT(result != 0, "too small buffer should fail");
    return 1;
}

TEST(test_deserialise_bad_header)
{
    uint8_t buffer[CQ_CERTIFICATE_SIZE];
    cq_certificate_t cert;

    memset(buffer, 0, CQ_CERTIFICATE_SIZE);
    /* Bad magic */
    buffer[0] = 'X';

    int result = cq_certificate_deserialise(buffer, CQ_CERTIFICATE_SIZE, &cert);

    ASSERT(result != 0, "bad header should fail deserialise");
    return 1;
}

/* ============================================================================
 * Bounds Satisfied Tests
 * ============================================================================ */

TEST(test_bounds_satisfied_true)
{
    cq_certificate_t cert;
    memset(&cert, 0, sizeof(cert));

    cert.epsilon_total_claimed = 1.0e-4;
    cert.epsilon_max_measured = 5.0e-5;  /* Less than claimed */

    ASSERT(cq_certificate_bounds_satisfied(&cert) == true, "should be satisfied");
    return 1;
}

TEST(test_bounds_satisfied_exact)
{
    cq_certificate_t cert;
    memset(&cert, 0, sizeof(cert));

    cert.epsilon_total_claimed = 1.0e-4;
    cert.epsilon_max_measured = 1.0e-4;  /* Exactly equal */

    ASSERT(cq_certificate_bounds_satisfied(&cert) == true, "exact should be satisfied");
    return 1;
}

TEST(test_bounds_satisfied_false)
{
    cq_certificate_t cert;
    memset(&cert, 0, sizeof(cert));

    cert.epsilon_total_claimed = 1.0e-4;
    cert.epsilon_max_measured = 2.0e-4;  /* Greater than claimed */

    ASSERT(cq_certificate_bounds_satisfied(&cert) == false, "exceeded should not be satisfied");
    return 1;
}

/* ============================================================================
 * Format Tests
 * ============================================================================ */

TEST(test_certificate_format)
{
    cq_certificate_builder_t builder;
    cq_certificate_t cert;
    cq_fault_flags_t faults;

    setup_complete_builder(&builder);
    cq_fault_clear(&faults);
    cq_certificate_build(&builder, &cert, &faults);

    char buffer[1024];
    int len = cq_certificate_format(&cert, buffer, sizeof(buffer));

    ASSERT(len > 0, "format should return positive length");
    ASSERT(strstr(buffer, "CQCR") != NULL, "should contain magic");
    ASSERT(strstr(buffer, "Q16.16") != NULL, "should contain format");
    ASSERT(strstr(buffer, "YES") != NULL, "should show bounds satisfied");
    ASSERT(strstr(buffer, "VALID") != NULL, "should show valid integrity");
    return 1;
}

/* ============================================================================
 * Timestamp Test
 * ============================================================================ */

TEST(test_timestamp)
{
    uint64_t ts1 = cq_get_timestamp();
    uint64_t ts2 = cq_get_timestamp();

    ASSERT(ts1 > 0, "timestamp should be positive");
    ASSERT(ts2 >= ts1, "timestamp should be monotonic");
    /* Sanity check: timestamp should be > 2020-01-01 (1577836800) */
    ASSERT(ts1 > 1577836800, "timestamp should be recent");
    return 1;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    printf("=== Certifiable-Quant Certificate Module Tests ===\n\n");

    /* Builder tests */
    RUN_TEST(test_builder_init);
    RUN_TEST(test_builder_set_version);
    RUN_TEST(test_builder_set_source_hash);
    RUN_TEST(test_builder_set_bn_info);
    RUN_TEST(test_builder_set_bn_info_no_bn);
    RUN_TEST(test_builder_is_complete_empty);
    RUN_TEST(test_builder_is_complete_partial);
    RUN_TEST(test_builder_is_complete_full);

    /* Certificate build tests */
    RUN_TEST(test_certificate_build_success);
    RUN_TEST(test_certificate_build_claims);
    RUN_TEST(test_certificate_build_incomplete);
    RUN_TEST(test_certificate_build_null_inputs);

    /* Merkle root tests */
    RUN_TEST(test_merkle_root_computed);
    RUN_TEST(test_merkle_root_deterministic);
    RUN_TEST(test_merkle_root_changes_with_content);

    /* Integrity verification tests */
    RUN_TEST(test_verify_integrity_valid);
    RUN_TEST(test_verify_integrity_tampered);
    RUN_TEST(test_verify_header_valid);
    RUN_TEST(test_verify_header_bad_magic);

    /* Serialisation tests */
    RUN_TEST(test_serialise_deserialise_roundtrip);
    RUN_TEST(test_deserialise_too_small);
    RUN_TEST(test_deserialise_bad_header);

    /* Bounds satisfied tests */
    RUN_TEST(test_bounds_satisfied_true);
    RUN_TEST(test_bounds_satisfied_exact);
    RUN_TEST(test_bounds_satisfied_false);

    /* Format tests */
    RUN_TEST(test_certificate_format);

    /* Utility tests */
    RUN_TEST(test_timestamp);

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
