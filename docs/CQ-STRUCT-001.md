# CQ-STRUCT-001: Data Structure Specification

**Project:** Certifiable-Quant  
**Document ID:** CQ-STRUCT-001  
**Version:** 1.0  
**Status:** Final (Audit-Grade)  
**Date:** January 2026  
**Author:** William Murray  
**Classification:** Technical Specification

---

## Document Control

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | Jan 2026 | W. Murray | Initial release (audit-grade) |

**Traceability:**
- Parent: CQ-MATH-001 (Mathematical Foundations)
- Children: SRS-001 through SRS-005
- Related: CT-STRUCT-001 (certifiable-training), CI-STRUCT-001 (certifiable-inference)

---

## §0 Design Principles

### §0.1 Direct Mathematical Mapping

Every struct field must trace to a variable in CQ-MATH-001. This ensures the implementation is a literal transcription of the mathematics.

| Struct Field | Mathematical Symbol | CQ-MATH-001 Reference |
|--------------|--------------------|-----------------------|
| `epsilon_total` | ε_total | §3.6.3 |
| `amp_factor` | A_l | §3.6.2 |
| `entry_error` | ε₀ | §3.6.1 |

### §0.2 Immutability

Once an analysis or calibration phase is complete, the resulting data structures must be treated as read-only to preserve the audit trail. Modification after completion invalidates the certificate chain.

### §0.3 Canonical Layout

All structures intended for serialization (Certificates, Model Artifacts) must have:
- Fixed-width types (no `int`, `long`, `size_t`)
- Explicit padding (no implicit compiler padding)
- Little-endian byte ordering
- Deterministic field ordering

This ensures bit-exact hashing across platforms (SRS-005).

### §0.4 Alignment Requirements

All major structures must be 8-byte aligned to support:
- Efficient access on 64-bit architectures (x86-64, ARM64, RISC-V)
- Strictly deterministic hashing
- Memory-mapped I/O compatibility

**Critical:** Any `uint64_t` field must be preceded by explicit padding if the natural offset would not be 8-byte aligned. This prevents implicit compiler padding that would violate §0.3.

### §0.5 Certificate Chain Architecture

The following diagram illustrates how data structures interlock to form the auditable certificate chain:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           cq_certificate_t                                  │
│                         (Root of Trust - §7)                                │
├─────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐              │
│  │ analysis_digest │  │calibration_digest│  │verification_digest│           │
│  │    [32 bytes]   │  │    [32 bytes]   │  │    [32 bytes]   │              │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘              │
│           │                    │                    │                       │
│           ▼                    ▼                    ▼                       │
│  ┌─────────────────────────────────────────────────────────────┐            │
│  │                      merkle_root [32]                       │            │
│  │              SHA-256(all sections above)                    │            │
│  └─────────────────────────────────────────────────────────────┘            │
│                                │                                            │
│                                ▼                                            │
│  ┌─────────────────────────────────────────────────────────────┐            │
│  │                      signature [64]                         │            │
│  │                  Ed25519 (optional)                         │            │
│  └─────────────────────────────────────────────────────────────┘            │
└─────────────────────────────────────────────────────────────────────────────┘
                                 │
          ┌──────────────────────┼──────────────────────┐
          │                      │                      │
          ▼                      ▼                      ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│ cq_analysis_ctx │    │cq_calibration_  │    │cq_verification_ │
│      (§3)       │    │   report (§4)   │    │   report (§6)   │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ • entry_error   │    │ • dataset_hash  │    │ • ε_theoretical │
│   (ε₀)          │    │ • coverage_min  │    │ • ε_measured    │
│ • total_error   │    │ • range_veto    │    │ • bounds_pass   │
│   (ε_total)     │    │ • tensor_stats  │    │ • layer_diffs   │
│ • layer_count   │    └────────┬────────┘    └────────┬────────┘
│ • layers[]      │             │                      │
└────────┬────────┘             │                      │
         │                      │                      │
         ▼                      │                      │
┌─────────────────┐             │                      │
│cq_layer_contract│             │                      │
│    (§3.2)       │             │                      │
├─────────────────┤             │                      │
│ • amp_factor    │◄────────────┼──────────────────────┘
│   (A_l = ‖W‖)   │             │        (Verification compares
│ • weight_error  │             │         measured vs contract)
│ • bias_error    │             │
│ • proj_error    │             │
│ • overflow_proof│             │
└────────┬────────┘             │
         │                      │
         ▼                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                     cq_model_header_t (§5.4)                    │
│                    (Quantised Model Binary)                     │
├─────────────────────────────────────────────────────────────────┤
│  source_model_hash ──► Links to FP32 origin                     │
│  certificate_hash  ──► Links back to certificate                │
│                                                                 │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │cq_layer_header_t│  │cq_layer_header_t│  │cq_layer_header_t│  │
│  │   Layer 0       │  │   Layer 1       │  │   Layer N       │  │
│  ├─────────────────┤  ├─────────────────┤  ├─────────────────┤  │
│  │ weight_spec     │  │ weight_spec     │  │ weight_spec     │  │
│  │ input_spec      │  │ input_spec      │  │ input_spec      │  │
│  │ bias_spec       │  │ bias_spec       │  │ bias_spec       │  │
│  │ dyadic_valid ───┼──┼─► Runtime check:│S_b = S_w × S_x     │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

**Chain Integrity Properties:**

1. **Certificate → Model:** `target_model_hash` in certificate must match SHA-256 of model binary
2. **Model → Certificate:** `certificate_hash` in model header must match SHA-256 of certificate
3. **Analysis → Certificate:** `analysis_digest` permanently binds ε_total proof to binary
4. **Calibration → Certificate:** `calibration_digest` binds dataset coverage to binary
5. **Verification → Certificate:** `verification_digest` binds measured ≤ theoretical proof

---

## §1 Primitive Types

### §1.1 Fixed-Point Storage Types

**Requirement ID:** ST-001-A  
**Traceability:** CQ-MATH-001 §2.1

```c
/**
 * @file cq_types.h
 * @project Certifiable-Quant
 * @brief Core fixed-point type definitions.
 *
 * @traceability CQ-MATH-001 §2.1, ST-001
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304
 */

#ifndef CQ_TYPES_H
#define CQ_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Fixed-Point Storage Types
 * ============================================================================ */

/**
 * @brief Q16.16 fixed-point: 16 integer bits, 16 fractional bits.
 * @details Range: [-32768, +32767.99998], Step: 2^-16 ≈ 1.53×10^-5
 * @traceability CQ-MATH-001 §2.1
 */
typedef int32_t cq_fixed16_t;

/**
 * @brief Q8.24 fixed-point: 8 integer bits, 24 fractional bits.
 * @details Range: [-128, +127.99999994], Step: 2^-24 ≈ 5.96×10^-8
 * @traceability CQ-MATH-001 §2.1
 */
typedef int32_t cq_fixed24_t;

/**
 * @brief Q32.32 accumulator: 32 integer bits, 32 fractional bits.
 * @details Used for intermediate accumulation to prevent overflow.
 * @traceability CQ-MATH-001 §2.1, §3.4
 */
typedef int64_t cq_accum64_t;

#endif /* CQ_TYPES_H */
```

### §1.2 Scale Representation

**Requirement ID:** ST-001-B  
**Traceability:** CQ-MATH-001 §2.1, §3.2

```c
/**
 * @brief Scale exponent for power-of-two scales.
 * @details Represents Scale S = 2^exponent.
 *          Valid range: [-32, +31] for practical quantization.
 * @traceability CQ-MATH-001 §3.2 (Dyadic Constraint)
 */
typedef int8_t cq_scale_exp_t;

/**
 * @brief Fixed-point format enumeration.
 * @traceability CQ-MATH-001 §2.1
 */
typedef enum {
    CQ_FORMAT_Q16_16 = 0,  /**< Q16.16: 16 fractional bits */
    CQ_FORMAT_Q8_24  = 1,  /**< Q8.24: 24 fractional bits */
    CQ_FORMAT_Q32_32 = 2   /**< Q32.32: 32 fractional bits (accumulator) */
} cq_format_t;
```

### §1.3 Fixed-Point Constants

**Requirement ID:** ST-001-C  
**Traceability:** CQ-MATH-001 §2.1

```c
/* ============================================================================
 * Q16.16 Constants
 * ============================================================================ */

#define CQ_Q16_SHIFT      16
#define CQ_Q16_ONE        ((cq_fixed16_t)0x00010000)  /* 65536 */
#define CQ_Q16_HALF       ((cq_fixed16_t)0x00008000)  /* 32768 */
#define CQ_Q16_MAX        ((cq_fixed16_t)0x7FFFFFFF)  /* INT32_MAX */
#define CQ_Q16_MIN        ((cq_fixed16_t)0x80000000)  /* INT32_MIN */
#define CQ_Q16_EPS        ((cq_fixed16_t)1)           /* Smallest step */

/* ============================================================================
 * Q8.24 Constants
 * ============================================================================ */

#define CQ_Q24_SHIFT      24
#define CQ_Q24_ONE        ((cq_fixed24_t)16777216)    /* 2^24 */
#define CQ_Q24_HALF       ((cq_fixed24_t)8388608)     /* 2^23 */
#define CQ_Q24_MAX        ((cq_fixed24_t)0x7FFFFFFF)  /* INT32_MAX */
#define CQ_Q24_MIN        ((cq_fixed24_t)0x80000000)  /* INT32_MIN */
#define CQ_Q24_EPS        ((cq_fixed24_t)1)           /* Smallest step */
```

---

## §2 Fault Management

### §2.1 Fault Flags Structure

**Requirement ID:** ST-002  
**Traceability:** CQ-MATH-001 §10.1

```c
/**
 * @brief Fault flags for pipeline error tracking.
 * @details Bit-field structure allowing bitwise accumulation of errors.
 *          Each flag corresponds to a specific failure mode in CQ-MATH-001 §10.
 *
 * @traceability CQ-MATH-001 §10.1
 * @compliance MISRA-C:2012 Rule 6.1 (bit-fields shall be unsigned)
 */
typedef struct {
    uint32_t overflow        : 1;  /**< Saturated high (§2.4) */
    uint32_t underflow       : 1;  /**< Saturated low (§2.4) */
    uint32_t div_zero        : 1;  /**< Division by zero (§4.2.3) */
    uint32_t range_exceed    : 1;  /**< Calibration over-range veto (§5.2) */
    uint32_t unfolded_bn     : 1;  /**< BatchNorm not folded (§8.3) */
    uint32_t asymmetric      : 1;  /**< Non-symmetric quantization (§2.5) */
    uint32_t bound_violation : 1;  /**< Measured > theoretical (§7.2) */
    uint32_t _reserved       : 25; /**< Reserved for future use */
} cq_fault_flags_t;

/**
 * @brief Check if any fault flag is set.
 * @param f Pointer to fault flags structure.
 * @return true if any fault is set, false otherwise.
 */
static inline bool cq_has_fault(const cq_fault_flags_t *f) {
    return f->overflow || f->underflow || f->div_zero ||
           f->range_exceed || f->unfolded_bn || f->asymmetric ||
           f->bound_violation;
}

/**
 * @brief Check if any fail-closed fault is set.
 * @details Fail-closed faults prevent certificate generation.
 * @param f Pointer to fault flags structure.
 * @return true if any fail-closed fault is set.
 * @traceability CQ-MATH-001 §10.2
 */
static inline bool cq_has_fatal_fault(const cq_fault_flags_t *f) {
    return f->div_zero || f->range_exceed || f->unfolded_bn ||
           f->asymmetric || f->bound_violation;
}

/**
 * @brief Clear all fault flags.
 * @param f Pointer to fault flags structure.
 */
static inline void cq_fault_clear(cq_fault_flags_t *f) {
    *((uint32_t *)f) = 0;
}

/**
 * @brief Merge fault flags (bitwise OR).
 * @param dst Destination fault flags (accumulated).
 * @param src Source fault flags to merge.
 */
static inline void cq_fault_merge(cq_fault_flags_t *dst,
                                   const cq_fault_flags_t *src) {
    *((uint32_t *)dst) |= *((uint32_t *)src);
}
```

### §2.2 Fault Codes

**Requirement ID:** ST-002-B  
**Traceability:** CQ-MATH-001 §10.2

```c
/**
 * @brief Fault code enumeration for error reporting.
 * @traceability CQ-MATH-001 §10.2
 */
typedef enum {
    CQ_FAULT_NONE                  = 0x00,
    CQ_FAULT_OVERFLOW              = 0x01,
    CQ_FAULT_UNDERFLOW             = 0x02,
    CQ_FAULT_DIV_ZERO              = 0x04,
    CQ_FAULT_RANGE_CONTRADICTION   = 0x08,
    CQ_FAULT_UNFOLDED_BATCHNORM    = 0x10,
    CQ_FAULT_ASYMMETRIC_PARAMS     = 0x20,
    CQ_FAULT_BOUND_VIOLATION       = 0x40
} cq_fault_code_t;
```

---

## §3 Analysis Structures (Module 1)

### §3.1 Overflow Proof

**Requirement ID:** ST-003-A  
**Traceability:** CQ-MATH-001 §3.4

```c
/**
 * @brief Overflow proof for accumulator safety.
 * @details Stores inputs used to prove |acc| < 2^63.
 *          Safety condition: n · |w_int|_max · |x_int|_max < 2^63
 *
 * @traceability CQ-MATH-001 §3.4
 * @note uint64_t requires 8-byte alignment; explicit _pad field prevents
 *       implicit compiler padding (§0.3 compliance).
 */
typedef struct {
    uint32_t max_weight_mag;    /**< |w_int|_max: Maximum weight magnitude */
    uint32_t max_input_mag;     /**< |x_int|_max: Maximum input magnitude */
    uint32_t dot_product_len;   /**< n: Length of dot product */
    uint32_t _pad;              /**< Explicit padding for 8-byte alignment */
    uint64_t safety_margin;     /**< 2^63 - (n · w · x): Remaining headroom */
    bool     is_safe;           /**< True if safety condition satisfied */
    uint8_t  _reserved[7];      /**< Padding to 8-byte alignment */
} cq_overflow_proof_t;

/**
 * @brief Verify overflow proof safety condition.
 * @param proof Pointer to overflow proof structure.
 * @return true if accumulator is guaranteed safe.
 */
static inline bool cq_overflow_is_safe(const cq_overflow_proof_t *proof) {
    uint64_t product = (uint64_t)proof->dot_product_len *
                       (uint64_t)proof->max_weight_mag *
                       (uint64_t)proof->max_input_mag;
    return product < ((uint64_t)1 << 63);
}
```

### §3.2 Layer Error Contract

**Requirement ID:** ST-003-B  
**Traceability:** CQ-MATH-001 §3.6.2

```c
/**
 * @brief Error contract for a single layer.
 * @details Stores closed-form recurrence parameters:
 *          ε_{l+1} ≤ A_l·ε_l + ‖ΔW_l‖·‖x_l‖ + ‖Δb_l‖ + ε_proj,l
 *
 * @traceability CQ-MATH-001 §3.6.2
 */
typedef struct {
    /* Layer identification */
    uint32_t layer_index;           /**< Layer index in network (0-based) */
    uint32_t layer_type;            /**< Layer type enumeration */

    /* Amplification factor */
    double amp_factor;              /**< A_l = ‖W_l‖ (operator norm) */

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

/**
 * @brief Layer type enumeration.
 */
typedef enum {
    CQ_LAYER_LINEAR     = 0,  /**< Fully connected / dense */
    CQ_LAYER_CONV2D     = 1,  /**< 2D convolution */
    CQ_LAYER_RELU       = 2,  /**< ReLU activation */
    CQ_LAYER_SOFTMAX    = 3,  /**< Softmax (output only) */
    CQ_LAYER_MAXPOOL    = 4,  /**< Max pooling */
    CQ_LAYER_AVGPOOL    = 5   /**< Average pooling */
} cq_layer_type_t;
```

### §3.3 Analysis Context

**Requirement ID:** ST-003-C  
**Traceability:** CQ-MATH-001 §3.6.1, §3.6.3

```c
/**
 * @brief Complete analysis context for a model.
 * @details Contains entry error, per-layer contracts, and total bound.
 *
 * @traceability CQ-MATH-001 §3.6.1 (entry error), §3.6.3 (total error)
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
 * @details Fixed-size structure containing analysis summary for hashing.
 *
 * @traceability CQ-MATH-001 §9.2
 */
typedef struct {
    double entry_error;             /**< ε₀ */
    double total_error_bound;       /**< ε_total */
    uint32_t layer_count;           /**< Number of layers */
    uint32_t overflow_safe_count;   /**< Layers with safe overflow proof */
    uint8_t layers_hash[32];        /**< SHA-256 of serialised layer contracts */
} cq_analysis_digest_t;
```

---

## §4 Calibration Structures (Module 2)

### §4.1 Tensor Statistics

**Requirement ID:** ST-004-A  
**Traceability:** CQ-MATH-001 §5.1, §5.2

```c
/**
 * @brief Observed statistics for a single tensor.
 * @details Stores calibration observations and safe range claims.
 *
 * @traceability CQ-MATH-001 §5.1 (coverage), §5.2 (over-range veto)
 */
typedef struct {
    /* Tensor identification */
    uint32_t tensor_id;             /**< Unique tensor identifier */
    uint32_t layer_index;           /**< Parent layer index */

    /* Observed range (from calibration dataset) */
    float min_observed;             /**< L_obs: Minimum observed value */
    float max_observed;             /**< U_obs: Maximum observed value */

    /* Claimed safe range (for scaling) */
    float min_safe;                 /**< L_safe: Claimed minimum */
    float max_safe;                 /**< U_safe: Claimed maximum */

    /* Coverage metric */
    float coverage_ratio;           /**< C_t = observed_range / safe_range */

    /* Degenerate detection (§5.3) */
    bool is_degenerate;             /**< True if |max - min| < ε_degenerate */

    /* Range veto (§5.2) */
    bool range_veto;                /**< True if observed exceeds safe */

    uint8_t _reserved[2];           /**< Padding to 8-byte alignment */
} cq_tensor_stats_t;

/**
 * @brief Check if tensor passes range veto.
 * @details Verifies [L_obs, U_obs] ⊆ [L_safe, U_safe]
 * @param stats Pointer to tensor statistics.
 * @return true if range is valid (no veto).
 * @traceability CQ-MATH-001 §5.2
 */
static inline bool cq_tensor_range_valid(const cq_tensor_stats_t *stats) {
    return (stats->min_observed >= stats->min_safe) &&
           (stats->max_observed <= stats->max_safe);
}
```

### §4.2 Calibration Configuration

**Requirement ID:** ST-004-B  
**Traceability:** CQ-MATH-001 §5.1

```c
/**
 * @brief Calibration configuration parameters.
 * @traceability CQ-MATH-001 §5.1
 */
typedef struct {
    float coverage_min_threshold;   /**< C_min threshold (default: 0.90) */
    float coverage_p10_threshold;   /**< C_p10 threshold (default: 0.95) */
    float degenerate_epsilon;       /**< ε_degenerate for §5.3 */
    uint32_t min_samples;           /**< Minimum calibration samples */
    uint32_t _reserved;             /**< Padding */
} cq_calibrate_config_t;

/**
 * @brief Default calibration configuration.
 */
#define CQ_CALIBRATE_CONFIG_DEFAULT { \
    .coverage_min_threshold = 0.90f, \
    .coverage_p10_threshold = 0.95f, \
    .degenerate_epsilon = 1e-7f, \
    .min_samples = 100, \
    ._reserved = 0 \
}
```

### §4.3 Calibration Report

**Requirement ID:** ST-004-C  
**Traceability:** CQ-MATH-001 §5.1, §5.2

```c
/**
 * @brief Complete calibration report.
 * @details Summary of calibration pass for certificate.
 *
 * @traceability CQ-MATH-001 §5.1
 */
typedef struct {
    /* Dataset identification */
    uint8_t dataset_hash[32];       /**< SHA-256 of calibration dataset */
    uint32_t sample_count;          /**< Number of calibration samples */
    uint32_t tensor_count;          /**< Number of tensors calibrated */

    /* Global coverage metrics */
    float global_coverage_min;      /**< C_min: Minimum across all tensors */
    float global_coverage_p10;      /**< C_p10: 10th percentile */
    float global_coverage_mean;     /**< Mean coverage */
    uint32_t _pad1;                 /**< Padding */

    /* Veto status */
    bool range_veto_triggered;      /**< True if any L_obs < L_safe or U_obs > U_safe */
    bool coverage_veto_triggered;   /**< True if C_min < threshold */
    uint8_t _reserved[6];           /**< Padding */

    /* Tensor statistics (caller-allocated) */
    cq_tensor_stats_t *tensors;     /**< Array of tensor stats [tensor_count] */

    /* Accumulated faults */
    cq_fault_flags_t faults;        /**< Accumulated fault flags */
    uint32_t _pad2;                 /**< Padding */
} cq_calibration_report_t;

/**
 * @brief Serialisable calibration digest (for certificate).
 * @traceability CQ-MATH-001 §9.2
 */
typedef struct {
    uint8_t dataset_hash[32];       /**< SHA-256 of calibration dataset */
    uint32_t sample_count;          /**< Number of samples */
    uint32_t tensor_count;          /**< Number of tensors */
    float global_coverage_min;      /**< C_min */
    float global_coverage_p10;      /**< C_p10 */
    uint8_t range_veto_status;      /**< 0 = pass, 1 = veto */
    uint8_t coverage_veto_status;   /**< 0 = pass, 1 = veto */
    uint8_t _reserved[6];           /**< Padding */
} cq_calibration_digest_t;
```

---

## §5 Conversion Structures (Module 3)

### §5.1 BatchNorm Folding Record

**Requirement ID:** ST-005-A  
**Traceability:** CQ-MATH-001 §8

```c
/**
 * @brief BatchNorm folding record for audit trail.
 * @details Records pre-fold and post-fold state for verification.
 *
 * @traceability CQ-MATH-001 §8.2, §8.3
 */
typedef struct {
    /* Pre-fold BN parameters */
    uint8_t original_bn_hash[32];   /**< SHA-256 of (γ, β, μ, σ²) */

    /* Post-fold weights */
    uint8_t folded_weights_hash[32]; /**< SHA-256 of (W', b') */

    /* Metadata */
    uint32_t layer_index;           /**< Layer where BN was folded */
    bool folding_occurred;          /**< True if folding was performed */
    uint8_t _reserved[3];           /**< Padding */
} cq_bn_folding_record_t;
```

### §5.2 Tensor Quantization Specification

**Requirement ID:** ST-005-B  
**Traceability:** CQ-MATH-001 §3.2 (Dyadic Constraint)

```c
/**
 * @brief Quantization specification for a tensor.
 * @details Defines scale, format, and symmetry constraint.
 *
 * @traceability CQ-MATH-001 §2.5 (symmetric only), §3.2 (dyadic)
 */
typedef struct {
    cq_scale_exp_t scale_exp;       /**< Exponent n for S = 2^n */
    uint8_t format;                 /**< cq_format_t: Q16.16 or Q8.24 */
    bool is_symmetric;              /**< Must be true (§2.5 scope lock) */
    uint8_t _reserved;              /**< Padding */
} cq_tensor_spec_t;

/**
 * @brief Verify tensor spec is symmetric (scope lock).
 * @param spec Pointer to tensor specification.
 * @return true if symmetric constraint satisfied.
 * @traceability CQ-MATH-001 §2.5
 */
static inline bool cq_tensor_is_symmetric(const cq_tensor_spec_t *spec) {
    return spec->is_symmetric;
}
```

### §5.3 Quantized Layer Header

**Requirement ID:** ST-005-C  
**Traceability:** CQ-MATH-001 §3.2

```c
/**
 * @brief Header for a quantized layer in the model file.
 * @details Contains quantization specs and dyadic constraint verification.
 *
 * @traceability CQ-MATH-001 §3.2 (Dyadic Constraint)
 */
typedef struct {
    /* Layer identification */
    uint32_t layer_index;           /**< Layer index (0-based) */
    uint32_t layer_type;            /**< cq_layer_type_t */

    /* Tensor specifications */
    cq_tensor_spec_t weight_spec;   /**< Weight quantization spec */
    cq_tensor_spec_t input_spec;    /**< Input quantization spec */
    cq_tensor_spec_t bias_spec;     /**< Bias quantization spec */
    cq_tensor_spec_t output_spec;   /**< Output quantization spec */

    /* Tensor dimensions */
    uint32_t weight_rows;           /**< Weight matrix rows */
    uint32_t weight_cols;           /**< Weight matrix columns */
    uint32_t bias_len;              /**< Bias vector length */
    uint32_t _pad;                  /**< Padding */

    /* Data offsets (within model file) */
    uint64_t weight_offset;         /**< Byte offset to weight data */
    uint64_t bias_offset;           /**< Byte offset to bias data */

    /* Dyadic constraint satisfaction */
    bool dyadic_valid;              /**< True if bias.scale == weight.scale + input.scale */
    uint8_t _reserved[7];           /**< Padding */
} cq_layer_header_t;

/**
 * @brief Verify dyadic constraint for layer.
 * @details Checks S_b ≡ S_w × S_x (bias scale = weight scale × input scale)
 * @param hdr Pointer to layer header.
 * @return true if dyadic constraint satisfied.
 * @traceability CQ-MATH-001 §3.2
 */
static inline bool cq_layer_dyadic_valid(const cq_layer_header_t *hdr) {
    int expected_bias_exp = hdr->weight_spec.scale_exp +
                            hdr->input_spec.scale_exp;
    return hdr->bias_spec.scale_exp == expected_bias_exp;
}
```

### §5.4 Quantized Model Header

**Requirement ID:** ST-005-D  
**Traceability:** CQ-MATH-001 §9.2

```c
/**
 * @brief Header for quantized model file (model_q16.bin).
 * @details Fixed-size header for deterministic loading.
 *
 * @traceability CQ-MATH-001 §9.2
 */
typedef struct {
    /* Magic and version */
    uint8_t magic[4];               /**< "CQ16" or "CQ24" */
    uint32_t version;               /**< Format version (1) */

    /* Model identity */
    uint8_t source_model_hash[32];  /**< SHA-256 of source FP32 model */
    uint8_t quantized_hash[32];     /**< SHA-256 of quantized weights */

    /* Structure */
    uint32_t layer_count;           /**< Number of layers */
    uint32_t total_params;          /**< Total parameter count */
    uint64_t total_size;            /**< Total file size in bytes */

    /* Layer headers offset */
    uint64_t headers_offset;        /**< Byte offset to layer headers array */

    /* Certificate reference */
    uint8_t certificate_hash[32];   /**< SHA-256 of associated certificate */

    /* Padding */
    uint8_t _reserved[24];          /**< Reserved for future use */
} cq_model_header_t;

#define CQ_MODEL_MAGIC_Q16  {'C', 'Q', '1', '6'}
#define CQ_MODEL_MAGIC_Q24  {'C', 'Q', '2', '4'}
#define CQ_MODEL_VERSION    1
```

---

## §6 Verification Structures (Module 4)

### §6.1 Layer Comparison Result

**Requirement ID:** ST-006-A  
**Traceability:** CQ-MATH-001 §7

```c
/**
 * @brief Comparison result for a single layer.
 * @details Stores measured vs theoretical error comparison.
 *
 * @traceability CQ-MATH-001 §7.1
 */
typedef struct {
    uint32_t layer_index;           /**< Layer index */
    uint32_t sample_count;          /**< Number of samples compared */

    /* Measured errors */
    double error_max_measured;      /**< Maximum measured error */
    double error_mean_measured;     /**< Mean measured error */
    double error_std_measured;      /**< Standard deviation */

    /* Theoretical bound */
    double error_bound_theoretical; /**< ε_l from analysis */

    /* Bound satisfaction */
    bool bound_satisfied;           /**< True if max_measured ≤ theoretical */
    uint8_t _reserved[7];           /**< Padding */
} cq_layer_comparison_t;
```

### §6.2 Verification Report

**Requirement ID:** ST-006-B  
**Traceability:** CQ-MATH-001 §7

```c
/**
 * @brief Complete verification report.
 * @traceability CQ-MATH-001 §7
 */
typedef struct {
    /* Dataset identification */
    uint8_t verification_set_hash[32]; /**< SHA-256 of verification dataset */
    uint32_t sample_count;          /**< Number of verification samples */
    uint32_t layer_count;           /**< Number of layers verified */

    /* End-to-end results */
    double total_error_theoretical; /**< ε_total from analysis */
    double total_error_max_measured;/**< Maximum measured end-to-end error */
    double total_error_mean;        /**< Mean end-to-end error */

    /* Bound satisfaction */
    bool all_bounds_satisfied;      /**< True if all layers pass */
    bool total_bound_satisfied;     /**< True if total error passes */
    uint8_t _reserved[6];           /**< Padding */

    /* Per-layer comparisons (caller-allocated) */
    cq_layer_comparison_t *layers;  /**< Array of comparisons [layer_count] */

    /* Accumulated faults */
    cq_fault_flags_t faults;        /**< Accumulated fault flags */
    uint32_t _pad;                  /**< Padding */
} cq_verification_report_t;

/**
 * @brief Serialisable verification digest (for certificate).
 * @traceability CQ-MATH-001 §9.2
 */
typedef struct {
    uint8_t verification_set_hash[32]; /**< Dataset hash */
    uint32_t sample_count;          /**< Number of samples */
    uint32_t layers_passed;         /**< Layers satisfying bounds */
    double total_error_theoretical; /**< ε_total claimed */
    double total_error_max_measured;/**< ε_max measured */
    uint8_t bounds_satisfied;       /**< 0 = fail, 1 = pass */
    uint8_t _reserved[7];           /**< Padding */
} cq_verification_digest_t;
```

---

## §7 Certificate Structure (Module 5)

### §7.1 Certificate Header

**Requirement ID:** ST-007-A  
**Traceability:** CQ-MATH-001 §9

```c
/**
 * @brief Certificate for quantized model.
 * @details Serialised proof object with Merkle-like hash chain.
 *          All hashes of previous sections are included in final root.
 *
 * @traceability CQ-MATH-001 §9.2
 * @note This structure is serialised. All fields are fixed-width,
 *       little-endian, with explicit padding.
 */
typedef struct {
    /* ========== 1. Metadata Header (16 bytes) ========== */
    uint8_t magic[4];               /**< "CQCR" (CQ Certificate) */
    uint8_t version[4];             /**< Tool version bytes */
    uint64_t timestamp;             /**< Unix timestamp (UTC) */

    /* ========== 2. Scope Declaration (8 bytes) ========== */
    uint8_t scope_symmetric_only;   /**< 0x01 = symmetric only (§2.5) */
    uint8_t scope_format;           /**< 0x00 = Q16.16, 0x01 = Q8.24 */
    uint8_t _scope_reserved[6];     /**< Reserved */

    /* ========== 3. Source Identity (72 bytes) ========== */
    uint8_t source_model_hash[32];  /**< SHA-256 of FP32 model */
    uint8_t bn_folding_hash[32];    /**< Hash of BN folding record */
    uint8_t bn_folding_status;      /**< 0x00 = no BN, 0x01 = folded */
    uint8_t _source_reserved[7];    /**< Reserved */

    /* ========== 4. Mathematical Core (96 bytes) ========== */
    uint8_t analysis_digest[32];    /**< Hash of cq_analysis_digest_t */
    uint8_t calibration_digest[32]; /**< Hash of cq_calibration_digest_t */
    uint8_t verification_digest[32];/**< Hash of cq_verification_digest_t */

    /* ========== 5. Claims (32 bytes) ========== */
    double epsilon_0_claimed;       /**< ε₀: Entry error */
    double epsilon_total_claimed;   /**< ε_total: Theoretical bound */
    double epsilon_max_measured;    /**< Maximum measured error */
    double _claims_reserved;        /**< Reserved */

    /* ========== 6. Target Identity (40 bytes) ========== */
    uint8_t target_model_hash[32];  /**< SHA-256 of quantized model */
    uint32_t target_param_count;    /**< Total parameters */
    uint32_t target_layer_count;    /**< Number of layers */

    /* ========== 7. Integrity (96 bytes) ========== */
    uint8_t merkle_root[32];        /**< SHA-256 of all above sections */
    uint8_t signature[64];          /**< Ed25519 signature (optional) */

} cq_certificate_t;

#define CQ_CERTIFICATE_MAGIC    {'C', 'Q', 'C', 'R'}
#define CQ_CERTIFICATE_SIZE     360  /* Fixed size in bytes */
```

### §7.2 Certificate Verification

**Requirement ID:** ST-007-B  
**Traceability:** CQ-MATH-001 §9

```c
/**
 * @brief Verify certificate integrity.
 * @param cert Pointer to certificate.
 * @param model Pointer to quantized model (for hash comparison).
 * @return true if certificate is valid and matches model.
 */
bool cq_certificate_verify(const cq_certificate_t *cert,
                           const cq_model_header_t *model);

/**
 * @brief Compute Merkle root for certificate.
 * @param cert Pointer to certificate (merkle_root field ignored).
 * @param out_hash Output buffer for computed hash (32 bytes).
 */
void cq_certificate_compute_merkle(const cq_certificate_t *cert,
                                   uint8_t out_hash[32]);
```

---

## §8 Memory Layout Constraints

### §8.1 Serialisation Rules

**Requirement ID:** ST-008-A  
**Traceability:** CQ-MATH-001 §6 (Determinism)

| Rule | Requirement |
|------|-------------|
| **Padding** | All structs must include explicit `_reserved` fields. Implicit compiler padding is forbidden in serialised structs. |
| **Endianness** | All serialised integer fields are Little-Endian. |
| **No Pointers** | Serialised structs must not contain pointers. Arrays use offset/size pairs. |
| **Alignment** | Major structures align to 8-byte boundaries. |
| **Fixed Width** | Use `int32_t`, `uint64_t`, etc. Never `int`, `long`, `size_t`. |

### §8.2 Hash Computation Rules

**Requirement ID:** ST-008-B  
**Traceability:** CQ-MATH-001 §9

```c
/**
 * @brief Canonical serialisation for hashing.
 * @details All structures must be serialised canonically before hashing:
 *          1. Fixed byte order (little-endian)
 *          2. No pointers (use offset/length)
 *          3. Explicit padding (zero-filled)
 *          4. Deterministic field order
 */

/**
 * @brief Serialise analysis digest for hashing.
 * @param digest Pointer to analysis digest.
 * @param buffer Output buffer (must be CQ_ANALYSIS_DIGEST_SIZE bytes).
 * @param size Output: actual bytes written.
 */
void cq_analysis_digest_serialise(const cq_analysis_digest_t *digest,
                                   uint8_t *buffer, size_t *size);

#define CQ_ANALYSIS_DIGEST_SIZE     56
#define CQ_CALIBRATION_DIGEST_SIZE  56
#define CQ_VERIFICATION_DIGEST_SIZE 56
```

---

## §9 Data Dictionary

### §9.1 Structure Summary

| Structure | Purpose | Math Ref | Mutability |
|-----------|---------|----------|------------|
| `cq_fault_flags_t` | Pipeline error tracking | §10.1 | Accumulate only |
| `cq_overflow_proof_t` | Accumulator safety proof | §3.4 | Immutable after Analyze |
| `cq_layer_contract_t` | Per-layer error bounds | §3.6.2 | Immutable after Analyze |
| `cq_analysis_ctx_t` | Complete analysis context | §3.6 | Immutable after Analyze |
| `cq_tensor_stats_t` | Calibration observations | §5.1, §5.2 | Immutable after Calibrate |
| `cq_calibration_report_t` | Calibration summary | §5.1 | Immutable after Calibrate |
| `cq_bn_folding_record_t` | BN folding audit trail | §8 | Immutable after Convert |
| `cq_tensor_spec_t` | Quantization parameters | §3.2 | Immutable after Convert |
| `cq_layer_header_t` | Quantized layer metadata | §3.2 | Immutable after Convert |
| `cq_model_header_t` | Quantized model metadata | §9.2 | Immutable |
| `cq_layer_comparison_t` | Per-layer verification | §7.1 | Immutable after Verify |
| `cq_verification_report_t` | Verification summary | §7 | Immutable after Verify |
| `cq_certificate_t` | Final proof object | §9 | Immutable |

### §9.2 Field Constraints

| Field | Constraint | Rationale |
|-------|------------|-----------|
| `amp_factor` | ≥ 1.0 | Operator norm is always ≥ 1 |
| `output_error_bound` | > `input_error_bound` | Error can only increase |
| `is_symmetric` | Must be `true` | Scope lock (§2.5) |
| `coverage_ratio` | [0.0, 1.0] | Ratio definition |
| `dyadic_valid` | Must be `true` | Dyadic constraint (§3.2) |

---

## §10 Traceability Matrix

| Requirement ID | Structure | CQ-MATH-001 Section | SRS Reference |
|----------------|-----------|---------------------|---------------|
| ST-001-A | `cq_fixed16_t`, etc. | §2.1 | SRS-001 §3.1 |
| ST-001-B | `cq_scale_exp_t` | §3.2 | SRS-003 §3.1 |
| ST-002 | `cq_fault_flags_t` | §10.1 | SRS-001 §3.5 |
| ST-003-A | `cq_overflow_proof_t` | §3.4 | SRS-001 §3.4 |
| ST-003-B | `cq_layer_contract_t` | §3.6.2 | SRS-001 §3.3 |
| ST-003-C | `cq_analysis_ctx_t` | §3.6 | SRS-001 §3.3 |
| ST-004-A | `cq_tensor_stats_t` | §5.1, §5.2 | SRS-002 §3.2 |
| ST-004-C | `cq_calibration_report_t` | §5.1 | SRS-002 §3.3 |
| ST-005-A | `cq_bn_folding_record_t` | §8 | SRS-003 §3.6 |
| ST-005-B | `cq_tensor_spec_t` | §3.2 | SRS-003 §3.1 |
| ST-005-C | `cq_layer_header_t` | §3.2 | SRS-003 §3.2 |
| ST-006-A | `cq_layer_comparison_t` | §7.1 | SRS-004 §3.1 |
| ST-006-B | `cq_verification_report_t` | §7 | SRS-004 §3.2 |
| ST-007-A | `cq_certificate_t` | §9.2 | SRS-005 §3.1 |

---

## §11 References

### §11.1 Internal Documents

| ID | Title |
|----|-------|
| CQ-MATH-001 | Mathematical Foundations (Parent) |
| SRS-001-ANALYZE | Analysis Module Requirements |
| SRS-002-CALIBRATE | Calibration Module Requirements |
| SRS-003-CONVERT | Conversion Module Requirements |
| SRS-004-VERIFY | Verification Module Requirements |
| SRS-005-CERTIFICATE | Certificate Module Requirements |

### §11.2 Related Certifiable Documents

| ID | Title | Project |
|----|-------|---------|
| CT-STRUCT-001 | Data Structure Specification | certifiable-training |
| CI-STRUCT-001 | Data Structure Specification | certifiable-inference |
| CD-STRUCT-001 | Data Structure Specification | certifiable-data |

---

## Document Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Author | William Murray | | |
| Technical Review | | | |
| Quality Assurance | | | |

---

**Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.**

*This document is controlled. Printed copies are uncontrolled.*
