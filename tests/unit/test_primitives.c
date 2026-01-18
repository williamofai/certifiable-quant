/**
 * @file test_primitives.c
 * @project Certifiable-Quant
 * @brief Unit tests for DVM primitive operations.
 *
 * @traceability CQ-MATH-001 §2.3 (RNE Test Vectors)
 */

#include "dvm.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { printf("  FAIL: %s\n", msg); return 1; } \
    else { tests_passed++; printf("  PASS: %s\n", msg); } \
} while(0)

/* ============================================================================
 * Test: RNE Rounding (CQ-MATH-001 §2.3)
 * ============================================================================ */

int test_rne_rounding(void) {
    printf("\n=== Test: RNE Rounding ===\n");
    cq_fault_flags_t f = {0};
    int32_t r;

    /* TV-RNE-01: 1.5 -> 2 */
    r = cq_round_shift_rne(0x00018000LL, 16, &f);
    TEST(r == 2, "1.5 rounds to 2 (even)");

    /* TV-RNE-02: 2.5 -> 2 */
    r = cq_round_shift_rne(0x00028000LL, 16, &f);
    TEST(r == 2, "2.5 rounds to 2 (even)");

    /* TV-RNE-03: 3.5 -> 4 */
    r = cq_round_shift_rne(0x00038000LL, 16, &f);
    TEST(r == 4, "3.5 rounds to 4 (even)");

    /* TV-RNE-04: -1.5 -> -2 */
    r = cq_round_shift_rne((int64_t)0xFFFFFFFFFFFE8000LL, 16, &f);
    TEST(r == -2, "-1.5 rounds to -2 (even)");

    /* TV-RNE-05: -2.5 -> -2 */
    r = cq_round_shift_rne((int64_t)0xFFFFFFFFFFFD8000LL, 16, &f);
    TEST(r == -2, "-2.5 rounds to -2 (even)");

    return 0;
}

/* ============================================================================
 * Test: Overflow Safety
 * ============================================================================ */

int test_overflow_safety(void) {
    printf("\n=== Test: Overflow Safety ===\n");
    cq_overflow_proof_t p;

    p.max_weight_mag = 32767;
    p.max_input_mag = 32767;
    p.dot_product_len = 1000;
    TEST(cq_overflow_is_safe(&p), "1000 MACs safe");

    p.dot_product_len = (1 << 20);
    TEST(cq_overflow_is_safe(&p), "1M MACs safe");

    /* Zero fan-in edge case (IMPL-WATCH-02) */
    p.dot_product_len = 0;
    TEST(cq_overflow_is_safe(&p), "Zero fan-in trivially safe");

    return 0;
}

/* ============================================================================
 * Test: Q16.16 Multiplication
 * ============================================================================ */

int test_multiplication(void) {
    printf("\n=== Test: Q16.16 Multiplication ===\n");
    cq_fault_flags_t f = {0};
    cq_fixed16_t r;

    r = cq_mul_q16(CQ_Q16_ONE, CQ_Q16_ONE, &f);
    TEST(r == CQ_Q16_ONE, "1.0 * 1.0 = 1.0");

    r = cq_mul_q16(2 * CQ_Q16_ONE, 3 * CQ_Q16_ONE, &f);
    TEST(r == 6 * CQ_Q16_ONE, "2.0 * 3.0 = 6.0");

    r = cq_mul_q16(CQ_Q16_HALF, CQ_Q16_HALF, &f);
    TEST(r == CQ_Q16_ONE / 4, "0.5 * 0.5 = 0.25");

    return 0;
}

/* ============================================================================
 * Test: Saturation
 * ============================================================================ */

int test_saturation(void) {
    printf("\n=== Test: Saturation ===\n");
    cq_fault_flags_t f = {0};
    int32_t r;

    cq_fault_clear(&f);
    r = cq_clamp32(INT64_MAX, &f);
    TEST(r == INT32_MAX && f.overflow, "INT64_MAX saturates with overflow flag");

    cq_fault_clear(&f);
    r = cq_clamp32(INT64_MIN, &f);
    TEST(r == INT32_MIN && f.underflow, "INT64_MIN saturates with underflow flag");

    return 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("============================================\n");
    printf("Certifiable-Quant: DVM Primitives Tests\n");
    printf("============================================\n");

    int failed = 0;
    failed += test_rne_rounding();
    failed += test_overflow_safety();
    failed += test_multiplication();
    failed += test_saturation();

    printf("\n============================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    printf("============================================\n");

    return failed ? 1 : 0;
}
