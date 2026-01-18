/**
 * @file test_bit_identity.c
 * @project Certifiable-Quant
 * @brief Cross-platform bit-identity verification.
 *
 * @details Verifies that all operations produce identical bit patterns
 *          regardless of platform. Uses known test vectors.
 *
 * @traceability CQ-MATH-001 §6 (Determinism)
 */

#include "dvm.h"
#include "convert.h"
#include "sha256.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { printf("  FAIL: %s\n", msg); return 1; } \
    else { tests_passed++; printf("  PASS: %s\n", msg); } \
} while(0)

/* ============================================================================
 * Test: RNE Bit Patterns
 * ============================================================================ */

int test_rne_bit_patterns(void) {
    printf("\n=== Test: RNE Bit Patterns ===\n");
    cq_fault_flags_t f = {0};

    /* These must produce EXACT bit patterns on all platforms */
    TEST(cq_round_shift_rne(0x00018000LL, 16, &f) == 0x00000002, 
         "0x00018000 >> 16 = 0x00000002");
    TEST(cq_round_shift_rne(0x00028000LL, 16, &f) == 0x00000002, 
         "0x00028000 >> 16 = 0x00000002");
    TEST(cq_round_shift_rne(0x00038000LL, 16, &f) == 0x00000004, 
         "0x00038000 >> 16 = 0x00000004");

    return 0;
}

/* ============================================================================
 * Test: Multiplication Bit Patterns
 * ============================================================================ */

int test_mul_bit_patterns(void) {
    printf("\n=== Test: Multiplication Bit Patterns ===\n");
    cq_fault_flags_t f = {0};

    /* 1.0 * 1.0 = 1.0 in Q16.16 */
    cq_fixed16_t r = cq_mul_q16(0x00010000, 0x00010000, &f);
    TEST(r == 0x00010000, "0x00010000 * 0x00010000 = 0x00010000");

    /* 0.5 * 0.5 = 0.25 */
    r = cq_mul_q16(0x00008000, 0x00008000, &f);
    TEST(r == 0x00004000, "0x00008000 * 0x00008000 = 0x00004000");

    return 0;
}

/* ============================================================================
 * Test: SHA-256 Known Vector
 * ============================================================================ */

int test_sha256_vector(void) {
    printf("\n=== Test: SHA-256 Known Vector ===\n");
    
    /* SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 */
    uint8_t digest[32];
    cq_sha256("", 0, digest);
    
    uint8_t expected[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    
    TEST(memcmp(digest, expected, 32) == 0, "SHA-256('') matches known vector");
    
    /* SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad */
    cq_sha256("abc", 3, digest);
    
    uint8_t expected_abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    
    TEST(memcmp(digest, expected_abc, 32) == 0, "SHA-256('abc') matches known vector");
    
    return 0;
}

/* ============================================================================
 * Test: Quantization Bit Patterns
 * ============================================================================ */

int test_quantization_bit_patterns(void) {
    printf("\n=== Test: Quantization Bit Patterns ===\n");
    cq_fault_flags_t f = {0};

    /* 1.0 with scale 2^16 = 65536 = 0x00010000 */
    cq_fixed16_t r = cq_quantize_weight_rne(1.0f, 65536.0, &f);
    TEST(r == 0x00010000, "1.0 * 2^16 = 0x00010000");

    /* -1.0 with scale 2^16 = -65536 = 0xFFFF0000 */
    r = cq_quantize_weight_rne(-1.0f, 65536.0, &f);
    TEST(r == (cq_fixed16_t)0xFFFF0000, "-1.0 * 2^16 = 0xFFFF0000");

    return 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("============================================\n");
    printf("Certifiable-Quant: Bit Identity Tests\n");
    printf("============================================\n");

    int failed = 0;
    failed += test_rne_bit_patterns();
    failed += test_mul_bit_patterns();
    failed += test_sha256_vector();
    failed += test_quantization_bit_patterns();

    printf("\n============================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    printf("============================================\n");

    return failed ? 1 : 0;
}
