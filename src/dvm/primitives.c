/**
 * @file primitives.c
 * @project Certifiable-Quant
 * @brief DVM Primitive Operations Implementation
 *
 * @traceability CQ-MATH-001 §2-§3
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#include "dvm.h"
#include <math.h>

/* ============================================================================
 * Saturation Operations
 * ============================================================================ */

int32_t cq_clamp32(int64_t x, cq_fault_flags_t *faults)
{
    if (x > INT32_MAX) {
        if (faults) faults->overflow = 1;
        return INT32_MAX;
    }
    if (x < INT32_MIN) {
        if (faults) faults->underflow = 1;
        return INT32_MIN;
    }
    return (int32_t)x;
}

int64_t cq_add64_sat(int64_t a, int64_t b, cq_fault_flags_t *faults)
{
    if (b > 0 && a > INT64_MAX - b) {
        if (faults) faults->overflow = 1;
        return INT64_MAX;
    }
    if (b < 0 && a < INT64_MIN - b) {
        if (faults) faults->underflow = 1;
        return INT64_MIN;
    }
    return a + b;
}

int64_t cq_sub64_sat(int64_t a, int64_t b, cq_fault_flags_t *faults)
{
    if (b < 0 && a > INT64_MAX + b) {
        if (faults) faults->overflow = 1;
        return INT64_MAX;
    }
    if (b > 0 && a < INT64_MIN + b) {
        if (faults) faults->underflow = 1;
        return INT64_MIN;
    }
    return a - b;
}

/* ============================================================================
 * RNE Rounding
 * ============================================================================ */

int32_t cq_round_shift_rne(int64_t x, uint32_t shift, cq_fault_flags_t *faults)
{
    if (shift > 62) {
        if (faults) faults->overflow = 1;
        return 0;
    }
    if (shift == 0) {
        return cq_clamp32(x, faults);
    }

    int64_t divisor = (int64_t)1 << shift;
    int64_t half = divisor / 2;

    /* Use integer division for correct truncation toward zero */
    int64_t quot = x / divisor;
    int64_t remainder = x % divisor;

    /* RNE rounding logic */
    if (remainder > half) {
        /* Above halfway: round up (away from zero for positive) */
        quot += 1;
    } else if (remainder < -half) {
        /* Below -halfway: round down (away from zero for negative) */
        quot -= 1;
    } else if (remainder == half) {
        /* Exactly +halfway: round to even */
        quot += (quot & 1);
    } else if (remainder == -half) {
        /* Exactly -halfway: round to even */
        quot -= (quot & 1);
    }
    /* Otherwise: truncation toward zero is correct */

    return cq_clamp32(quot, faults);
}

/* ============================================================================
 * Fixed-Point Arithmetic
 * ============================================================================ */

cq_fixed16_t cq_mul_q16(cq_fixed16_t a, cq_fixed16_t b, cq_fault_flags_t *faults)
{
    int64_t wide = (int64_t)a * (int64_t)b;
    return cq_round_shift_rne(wide, CQ_Q16_SHIFT, faults);
}

cq_fixed16_t cq_div_q16(cq_fixed16_t a, cq_fixed16_t b, cq_fault_flags_t *faults)
{
    if (b == 0) {
        if (faults) faults->div_zero = 1;
        return 0;
    }

    int64_t wide_a = (int64_t)a << CQ_Q16_SHIFT;
    int64_t quot = wide_a / b;
    int64_t rem = wide_a % b;

    int64_t half_b = (b > 0) ? (b / 2) : ((-b) / 2);
    int64_t abs_rem = (rem >= 0) ? rem : -rem;

    if (abs_rem > half_b) {
        quot += (quot >= 0) ? 1 : -1;
    } else if (abs_rem == half_b) {
        if (quot & 1) {
            quot += (quot >= 0) ? 1 : -1;
        }
    }

    return cq_clamp32(quot, faults);
}

/* ============================================================================
 * Accumulator Operations
 * ============================================================================ */

void cq_mac_q16(cq_accum64_t *acc, cq_fixed16_t a, cq_fixed16_t b,
                cq_fault_flags_t *faults)
{
    int64_t product = (int64_t)a * (int64_t)b;
    *acc = cq_add64_sat(*acc, product, faults);
}

cq_fixed16_t cq_acc_to_q16(cq_accum64_t acc, cq_fault_flags_t *faults)
{
    return cq_round_shift_rne(acc, CQ_Q16_SHIFT, faults);
}

/* ============================================================================
 * Overflow Safety
 * ============================================================================ */

bool cq_overflow_is_safe(const cq_overflow_proof_t *proof)
{
    if (proof->dot_product_len == 0) {
        return true;
    }

    uint64_t product = (uint64_t)proof->dot_product_len *
                       (uint64_t)proof->max_weight_mag *
                       (uint64_t)proof->max_input_mag;
    return product < ((uint64_t)1 << 63);
}

/* ============================================================================
 * Portable Arithmetic Shift
 * ============================================================================ */

int32_t cq_sra32(int32_t v, int s)
{
#if defined(__GNUC__) || defined(__clang__)
    return v >> s;
#else
    if (v < 0 && s > 0) {
        return (v >> s) | ~(~0U >> s);
    }
    return v >> s;
#endif
}

int64_t cq_sra64(int64_t v, int s)
{
#if defined(__GNUC__) || defined(__clang__)
    return v >> s;
#else
    if (v < 0 && s > 0) {
        return (v >> s) | ~(~0ULL >> s);
    }
    return v >> s;
#endif
}
