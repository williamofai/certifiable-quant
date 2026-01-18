/**
 * @file cq_types.h
 * @project Certifiable-Quant
 * @brief Core type definitions for fixed-point quantization.
 *
 * @traceability CQ-MATH-001 §2, CQ-STRUCT-001 §1-§2
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * @license GPL-3.0 or Commercial (contact: william@fstopify.com)
 */

#ifndef CQ_TYPES_H
#define CQ_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Compile-Time Assertions (C99 compatible)
 * ============================================================================
 * Uses negative array size to fail compilation on constraint violation.
 * Traceability: CQ-STRUCT-001 §1.1 (Platform Assumptions)
 * ============================================================================ */

#define CQ_STATIC_ASSERT(cond, name) \
    typedef char cq_static_assert_##name[(cond) ? 1 : -1]

CQ_STATIC_ASSERT(sizeof(int32_t) == 4, int32_t_size);
CQ_STATIC_ASSERT(sizeof(int64_t) == 8, int64_t_size);
CQ_STATIC_ASSERT(sizeof(double) == 8, double_size);
CQ_STATIC_ASSERT(CHAR_BIT == 8, char_bit_value);

/* ============================================================================
 * Fixed-Point Storage Types (ST-001)
 * Traceability: CQ-MATH-001 §2.1, CQ-STRUCT-001 §1.1
 * ============================================================================ */

/** @brief Q16.16 fixed-point: 16 integer bits, 16 fractional bits */
typedef int32_t cq_fixed16_t;

/** @brief Q8.24 fixed-point: 8 integer bits, 24 fractional bits */
typedef int32_t cq_fixed24_t;

/** @brief Q32.32 accumulator for intermediate results */
typedef int64_t cq_accum64_t;

/** @brief Scale exponent for S = 2^n */
typedef int8_t cq_scale_exp_t;

/** @brief Fixed-point format enumeration */
typedef enum {
    CQ_FORMAT_Q16_16 = 0,
    CQ_FORMAT_Q8_24  = 1,
    CQ_FORMAT_Q32_32 = 2
} cq_format_t;

/* ============================================================================
 * Q16.16 Constants
 * ============================================================================ */

#define CQ_Q16_SHIFT    16
#define CQ_Q16_ONE      ((cq_fixed16_t)0x00010000)
#define CQ_Q16_HALF     ((cq_fixed16_t)0x00008000)
#define CQ_Q16_MAX      ((cq_fixed16_t)0x7FFFFFFF)
#define CQ_Q16_MIN      ((cq_fixed16_t)0x80000000)
#define CQ_Q16_EPS      ((cq_fixed16_t)1)

#define CQ_Q24_SHIFT    24
#define CQ_Q24_ONE      ((cq_fixed24_t)16777216)

/* ============================================================================
 * Fault Management (ST-002)
 * Traceability: CQ-MATH-001 §10.1, CQ-STRUCT-001 §2.1
 * ============================================================================ */

typedef struct {
    uint32_t overflow        : 1;
    uint32_t underflow       : 1;
    uint32_t div_zero        : 1;
    uint32_t range_exceed    : 1;
    uint32_t unfolded_bn     : 1;
    uint32_t asymmetric      : 1;
    uint32_t bound_violation : 1;
    uint32_t _reserved       : 25;
} cq_fault_flags_t;

typedef enum {
    CQ_FAULT_NONE               = 0x00,
    CQ_FAULT_OVERFLOW           = 0x01,
    CQ_FAULT_UNDERFLOW          = 0x02,
    CQ_FAULT_DIV_ZERO           = 0x04,
    CQ_FAULT_RANGE_EXCEED       = 0x08,
    CQ_FAULT_UNFOLDED_BN        = 0x10,
    CQ_FAULT_ASYMMETRIC_PARAMS  = 0x20,
    CQ_FAULT_BOUND_VIOLATION    = 0x40
} cq_fault_code_t;

/* Error codes */
#define CQ_ERROR_NULL_POINTER       (-1)
#define CQ_ERROR_DYADIC_VIOLATION   (-2)
#define CQ_ERROR_DIMENSION_MISMATCH (-3)

/* Fault helpers */
static inline bool cq_has_fault(const cq_fault_flags_t *f) {
    return f->overflow || f->underflow || f->div_zero ||
           f->range_exceed || f->unfolded_bn || f->asymmetric ||
           f->bound_violation;
}

static inline bool cq_has_fatal_fault(const cq_fault_flags_t *f) {
    return f->div_zero || f->range_exceed || f->unfolded_bn ||
           f->asymmetric || f->bound_violation;
}

static inline void cq_fault_clear(cq_fault_flags_t *f) {
    *((uint32_t *)f) = 0;
}

static inline void cq_fault_merge(cq_fault_flags_t *dst,
                                   const cq_fault_flags_t *src) {
    *((uint32_t *)dst) |= *((uint32_t *)src);
}

/* ============================================================================
 * Tensor Specification (ST-005-B)
 * ============================================================================ */

typedef struct {
    cq_scale_exp_t scale_exp;
    uint8_t format;
    bool is_symmetric;
    uint8_t _reserved;
} cq_tensor_spec_t;

/* ============================================================================
 * Overflow Proof (ST-003-A)
 * Traceability: CQ-MATH-001 §3.4
 * ============================================================================ */

typedef struct {
    uint32_t max_weight_mag;
    uint32_t max_input_mag;
    uint32_t dot_product_len;
    uint32_t _pad;              /* Explicit padding for alignment */
    uint64_t safety_margin;
    bool     is_safe;
    uint8_t  _reserved[7];
} cq_overflow_proof_t;

/* ============================================================================
 * Layer Header (ST-005-C)
 * ============================================================================ */

typedef enum {
    CQ_LAYER_LINEAR   = 0,
    CQ_LAYER_CONV2D   = 1,
    CQ_LAYER_RELU     = 2,
    CQ_LAYER_SOFTMAX  = 3,
    CQ_LAYER_MAXPOOL  = 4,
    CQ_LAYER_AVGPOOL  = 5
} cq_layer_type_t;

typedef struct {
    uint32_t layer_index;
    uint32_t layer_type;
    cq_tensor_spec_t weight_spec;
    cq_tensor_spec_t input_spec;
    cq_tensor_spec_t bias_spec;
    cq_tensor_spec_t output_spec;
    uint32_t weight_rows;
    uint32_t weight_cols;
    uint32_t bias_len;
    uint32_t _pad;
    uint64_t weight_offset;
    uint64_t bias_offset;
    bool dyadic_valid;
    uint8_t _reserved[7];
} cq_layer_header_t;

/* ============================================================================
 * BatchNorm Structures
 * ============================================================================ */

typedef struct {
    uint8_t original_bn_hash[32];
    uint8_t folded_weights_hash[32];
    uint32_t layer_index;
    bool folding_occurred;
    uint8_t _reserved[3];
} cq_bn_folding_record_t;

typedef struct {
    const float *gamma;
    const float *beta;
    const float *mean;
    const float *var;
    float epsilon;
    size_t channel_count;
} cq_bn_params_t;

#ifdef __cplusplus
}
#endif

#endif /* CQ_TYPES_H */
