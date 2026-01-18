/**
 * @file analyze.h
 * @project Certifiable-Quant
 * @brief Analysis module interface (The Theorist)
 *
 * Computes theoretical error bounds for neural network quantization
 * without executing inference. Static analysis on FP32 model graph.
 *
 * @traceability SRS-001-ANALYZE, CQ-MATH-001 §3, CQ-STRUCT-001 §3
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * @license GPL-3.0 or Commercial (contact: william@fstopify.com)
 */

#ifndef CQ_ANALYZE_H
#define CQ_ANALYZE_H

#include "cq_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Analysis configuration parameters.
 * @traceability SRS-001-ANALYZE §5.1
 */
typedef struct {
    cq_scale_exp_t input_scale_exp;     /**< Input quantization scale exponent */
    cq_scale_exp_t default_weight_exp;  /**< Default weight scale exponent */
    cq_scale_exp_t default_output_exp;  /**< Default output scale exponent */
    uint8_t        target_format;       /**< CQ_FORMAT_Q16_16 or CQ_FORMAT_Q8_24 */
    bool           allow_mixed_precision; /**< Allow Q8.24 for sensitive layers */
    bool           allow_chunked_accum;   /**< Allow chunked accumulation */
    uint8_t        _reserved[2];        /**< Padding */
} cq_analyze_config_t;

/**
 * @brief Default analysis configuration (Q16.16).
 */
#define CQ_ANALYZE_CONFIG_DEFAULT { \
    .input_scale_exp = 16, \
    .default_weight_exp = 16, \
    .default_output_exp = 16, \
    .target_format = CQ_FORMAT_Q16_16, \
    .allow_mixed_precision = false, \
    .allow_chunked_accum = false, \
    ._reserved = {0, 0} \
}

/* ============================================================================
 * Range Structures (FR-ANA-01)
 * ============================================================================ */

/**
 * @brief Value range for interval arithmetic.
 * @traceability SRS-001-ANALYZE FR-ANA-01
 */
typedef struct {
    double min_val;     /**< Minimum value in range */
    double max_val;     /**< Maximum value in range */
} cq_range_t;

/**
 * @brief Compute range magnitude (max absolute value).
 */
static inline double cq_range_magnitude(const cq_range_t *r) {
    double abs_min = (r->min_val < 0) ? -r->min_val : r->min_val;
    double abs_max = (r->max_val < 0) ? -r->max_val : r->max_val;
    return (abs_min > abs_max) ? abs_min : abs_max;
}

/* ============================================================================
 * Overflow Proof (ST-003-A)
 * Traceability: CQ-MATH-001 §3.4, CQ-STRUCT-001 §3.1
 * ============================================================================ */

/* cq_overflow_proof_t is defined in cq_types.h */

/**
 * @brief Compute overflow proof for a layer.
 *
 * @param max_weight_mag  Maximum weight magnitude (integer representation).
 * @param max_input_mag   Maximum input magnitude (integer representation).
 * @param dot_product_len Fan-in (number of MAC operations).
 * @param proof           Output: Overflow proof structure.
 * @return                true if safe, false if overflow possible.
 *
 * @traceability SRS-001-ANALYZE FR-ANA-02, CQ-MATH-001 §3.4
 */
bool cq_compute_overflow_proof(uint32_t max_weight_mag,
                               uint32_t max_input_mag,
                               uint32_t dot_product_len,
                               cq_overflow_proof_t *proof);

/* ============================================================================
 * Layer Contract (ST-003-B)
 * Traceability: CQ-MATH-001 §3.6.2, CQ-STRUCT-001 §3.2
 * ============================================================================ */

/**
 * @brief Error contract for a single layer.
 * @traceability CQ-MATH-001 §3.6.2, CQ-STRUCT-001 §3.2
 */
typedef struct {
    /* Layer identification */
    uint32_t layer_index;           /**< Layer index in network (0-based) */
    uint32_t layer_type;            /**< Layer type enumeration */

    /* Dimensions */
    uint32_t fan_in;                /**< Number of input connections */
    uint32_t fan_out;               /**< Number of output connections */

    /* Ranges (from interval arithmetic) */
    cq_range_t weight_range;        /**< [w_min, w_max] */
    cq_range_t input_range;         /**< [x_min, x_max] (theoretical) */
    cq_range_t output_range;        /**< [y_min, y_max] (computed) */

    /* Amplification factor */
    double amp_factor;              /**< A_l = ‖W_l‖ (operator norm upper bound) */

    /* Error contributions (static terms) */
    double weight_error_contrib;    /**< ‖ΔW_l‖ · ‖x_l‖ */
    double bias_error_contrib;      /**< ‖Δb_l‖ */
    double projection_error;        /**< ε_proj,l (requantization) */

    /* Computed bounds */
    double local_error_sum;         /**< Sum of static error terms */
    double input_error_bound;       /**< ε_l (from previous layer) */
    double output_error_bound;      /**< ε_{l+1} (computed) */

    /* Overflow proof for this layer */
    cq_overflow_proof_t overflow_proof;

    /* Validation */
    bool is_valid;                  /**< True if contract is complete */
    uint8_t _reserved[7];           /**< Padding to 8-byte alignment */
} cq_layer_contract_t;

/* ============================================================================
 * Analysis Context (ST-003-C)
 * Traceability: CQ-MATH-001 §3.6, CQ-STRUCT-001 §3.3
 * ============================================================================ */

/**
 * @brief Complete analysis context for a model.
 * @traceability CQ-MATH-001 §3.6, CQ-STRUCT-001 §3.3
 */
typedef struct {
    /* Entry error (base case) */
    double entry_error;             /**< ε₀: Input ingress quantization error */
    cq_scale_exp_t input_scale_exp; /**< Exponent for input scale S_in */
    uint8_t _pad1[7];               /**< Padding */

    /* Network structure */
    uint32_t layer_count;           /**< Number of layers */
    uint32_t _pad2;                 /**< Padding */

    /* Per-layer contracts (caller-allocated) */
    cq_layer_contract_t *layers;    /**< Array of layer contracts [layer_count] */

    /* Total error bound */
    double total_error_bound;       /**< ε_total: End-to-end bound */

    /* Validation */
    bool is_complete;               /**< True if all layers analysed */
    bool is_valid;                  /**< True if no fatal errors */
    uint8_t _reserved[6];           /**< Padding to 8-byte alignment */

    /* Accumulated faults */
    cq_fault_flags_t faults;        /**< Accumulated fault flags */
    uint32_t _pad3;                 /**< Padding */
} cq_analysis_ctx_t;

/**
 * @brief Serialisable analysis digest (for certificate).
 * @traceability CQ-MATH-001 §9.2, CQ-STRUCT-001 §3.3
 */
typedef struct {
    double entry_error;             /**< ε₀ */
    double total_error_bound;       /**< ε_total */
    uint32_t layer_count;           /**< Number of layers */
    uint32_t overflow_safe_count;   /**< Layers with safe overflow proof */
    uint8_t layers_hash[32];        /**< SHA-256 of serialised layer contracts */
} cq_analysis_digest_t;

/* ============================================================================
 * FR-ANA-01: Range Propagation
 * ============================================================================ */

/**
 * @brief Compute weight range from weight array.
 *
 * @param weights  Weight array (FP32).
 * @param count    Number of weights.
 * @param range    Output: Weight range.
 *
 * @traceability SRS-001-ANALYZE FR-ANA-01
 */
void cq_compute_weight_range(const float *weights,
                             size_t count,
                             cq_range_t *range);

/**
 * @brief Propagate range through linear layer using interval arithmetic.
 *
 * @param input_range   Input value range.
 * @param weight_range  Weight value range.
 * @param bias_range    Bias value range (may be NULL for no bias).
 * @param fan_in        Number of input connections.
 * @param output_range  Output: Computed output range.
 *
 * @traceability SRS-001-ANALYZE FR-ANA-01, CQ-MATH-001 §3.4
 */
void cq_propagate_range_linear(const cq_range_t *input_range,
                               const cq_range_t *weight_range,
                               const cq_range_t *bias_range,
                               uint32_t fan_in,
                               cq_range_t *output_range);

/**
 * @brief Propagate range through ReLU (clamp negative to zero).
 *
 * @param input_range   Input value range.
 * @param output_range  Output: Computed output range.
 *
 * @traceability SRS-001-ANALYZE FR-ANA-01
 */
void cq_propagate_range_relu(const cq_range_t *input_range,
                             cq_range_t *output_range);

/* ============================================================================
 * FR-ANA-03: Operator Norm Computation
 * ============================================================================ */

/**
 * @brief Compute Frobenius norm of weight matrix.
 *
 * @param weights  Weight matrix (row-major).
 * @param rows     Number of rows.
 * @param cols     Number of columns.
 * @return         ‖W‖_F = √(Σᵢⱼ wᵢⱼ²)
 *
 * @traceability SRS-001-ANALYZE FR-ANA-03, CQ-MATH-001 §3.6.2
 */
double cq_frobenius_norm(const float *weights, size_t rows, size_t cols);

/**
 * @brief Compute row-sum norm (L∞ induced norm) of weight matrix.
 *
 * @param weights  Weight matrix (row-major).
 * @param rows     Number of rows.
 * @param cols     Number of columns.
 * @return         max_i Σⱼ |wᵢⱼ|
 *
 * @traceability SRS-001-ANALYZE FR-ANA-03
 */
double cq_row_sum_norm(const float *weights, size_t rows, size_t cols);

/* ============================================================================
 * FR-ANA-04: Error Recurrence
 * ============================================================================ */

/**
 * @brief Compute error contributions for a layer.
 *
 * @param contract      Layer contract to populate.
 * @param weight_scale  Weight quantization scale (S_w = 2^exp).
 * @param output_scale  Output quantization scale (S_out = 2^exp).
 * @param max_input_norm Maximum input norm ‖x‖_max.
 *
 * @traceability SRS-001-ANALYZE FR-ANA-04, CQ-MATH-001 §3.6.2
 */
void cq_compute_error_contributions(cq_layer_contract_t *contract,
                                    double weight_scale,
                                    double output_scale,
                                    double max_input_norm);

/**
 * @brief Apply error recurrence to compute output error bound.
 *
 * @param contract          Layer contract to update.
 * @param input_error_bound ε_l from previous layer (or entry_error for layer 0).
 *
 * @traceability SRS-001-ANALYZE FR-ANA-04, CQ-MATH-001 §3.6.2
 */
void cq_apply_error_recurrence(cq_layer_contract_t *contract,
                               double input_error_bound);

/* ============================================================================
 * FR-ANA-05: Entry Error
 * ============================================================================ */

/**
 * @brief Compute entry error for input quantization.
 *
 * @param input_scale_exp  Input scale exponent (S_in = 2^exp).
 * @return                 ε₀ = 1/(2·S_in)
 *
 * @traceability SRS-001-ANALYZE FR-ANA-05, CQ-MATH-001 §3.6.1
 */
double cq_compute_entry_error(cq_scale_exp_t input_scale_exp);

/* ============================================================================
 * Context Management
 * ============================================================================ */

/**
 * @brief Initialise analysis context.
 *
 * @param ctx         Analysis context to initialise.
 * @param layer_count Number of layers.
 * @param layers      Pre-allocated array of layer contracts.
 * @param config      Analysis configuration.
 *
 * @traceability SRS-001-ANALYZE §5.1
 */
void cq_analysis_ctx_init(cq_analysis_ctx_t *ctx,
                          uint32_t layer_count,
                          cq_layer_contract_t *layers,
                          const cq_analyze_config_t *config);

/**
 * @brief Initialise a layer contract.
 *
 * @param contract    Layer contract to initialise.
 * @param layer_index Index of the layer.
 * @param layer_type  Type of the layer.
 * @param fan_in      Number of input connections.
 * @param fan_out     Number of output connections.
 */
void cq_layer_contract_init(cq_layer_contract_t *contract,
                            uint32_t layer_index,
                            uint32_t layer_type,
                            uint32_t fan_in,
                            uint32_t fan_out);

/* ============================================================================
 * FR-ANA-06: Total Error & Digest
 * ============================================================================ */

/**
 * @brief Compute total error bound from completed layer analysis.
 *
 * @param ctx  Analysis context with all layers analysed.
 * @return     0 on success, negative error code on failure.
 *
 * @pre  All layer contracts have valid output_error_bound
 * @post ctx->total_error_bound = ctx->layers[layer_count-1].output_error_bound
 *
 * @traceability SRS-001-ANALYZE FR-ANA-06, CQ-MATH-001 §3.6.3
 */
int cq_compute_total_error(cq_analysis_ctx_t *ctx);

/**
 * @brief Generate analysis digest for certificate.
 *
 * @param ctx     Completed analysis context.
 * @param digest  Output: Analysis digest (fixed-size).
 * @return        0 on success, negative error code on failure.
 *
 * @pre  ctx->is_complete == true
 * @pre  ctx->is_valid == true
 *
 * @traceability SRS-001-ANALYZE §5.2, CQ-MATH-001 §9.2
 */
int cq_analysis_digest_generate(const cq_analysis_ctx_t *ctx,
                                cq_analysis_digest_t *digest);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if analysis passed (no fatal faults).
 */
static inline bool cq_analysis_passed(const cq_analysis_ctx_t *ctx) {
    return ctx->is_complete && ctx->is_valid && !cq_has_fatal_fault(&ctx->faults);
}

/**
 * @brief Get scale factor from exponent.
 */
static inline double cq_scale_from_exp(cq_scale_exp_t exp) {
    return (double)(1LL << exp);
}

#ifdef __cplusplus
}
#endif

#endif /* CQ_ANALYZE_H */
