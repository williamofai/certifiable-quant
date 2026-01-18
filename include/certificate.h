/**
 * @file certificate.h
 * @project Certifiable-Quant
 * @brief Certificate module interface (The Notary)
 *
 * Assembles the final proof object from all module digests,
 * computes Merkle root, and optionally signs the certificate.
 *
 * @traceability SRS-005-CERTIFICATE, CQ-MATH-001 §9, CQ-STRUCT-001 §7
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * @license GPL-3.0 or Commercial (contact: william@fstopify.com)
 */

#ifndef CQ_CERTIFICATE_H
#define CQ_CERTIFICATE_H

#include "cq_types.h"
#include "analyze.h"
#include "calibrate.h"
#include "verify.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CQ_CERTIFICATE_MAGIC        "CQCR"
#define CQ_CERTIFICATE_VERSION      1
#define CQ_CERTIFICATE_SIZE         360

#define CQ_SCOPE_SYMMETRIC_ONLY     0x01
#define CQ_FORMAT_Q16_16_CODE       0x00
#define CQ_FORMAT_Q8_24_CODE        0x01

/* ============================================================================
 * Certificate Structure (ST-007-A)
 * Traceability: CQ-MATH-001 §9.2, CQ-STRUCT-001 §7.1
 * ============================================================================ */

/**
 * @brief Certificate for quantized model.
 * 
 * Serialised proof object with Merkle-like hash chain.
 * All hashes of previous sections are included in final root.
 * Fixed-size structure (360 bytes) for deterministic handling.
 *
 * @traceability CQ-MATH-001 §9.2, CQ-STRUCT-001 §7.1
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
    uint8_t analysis_digest[32];    /**< SHA-256 of cq_analysis_digest_t */
    uint8_t calibration_digest[32]; /**< SHA-256 of cq_calibration_digest_t */
    uint8_t verification_digest[32];/**< SHA-256 of cq_verification_digest_t */

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
    uint8_t signature[64];          /**< Ed25519 signature (optional, zeros if unsigned) */

} cq_certificate_t;

/* ============================================================================
 * Certificate Builder
 * ============================================================================ */

/**
 * @brief Certificate builder context.
 * 
 * Accumulates inputs before final certificate generation.
 */
typedef struct {
    /* Source model info */
    uint8_t source_model_hash[32];
    bool source_model_hash_set;

    /* BN folding info */
    uint8_t bn_folding_hash[32];
    bool bn_folded;
    bool bn_info_set;

    /* Digests */
    cq_analysis_digest_t analysis_digest;
    bool analysis_set;

    cq_calibration_digest_t calibration_digest;
    bool calibration_set;

    cq_verification_digest_t verification_digest;
    bool verification_set;

    /* Target model info */
    uint8_t target_model_hash[32];
    uint32_t target_param_count;
    uint32_t target_layer_count;
    bool target_set;

    /* Configuration */
    uint8_t scope_format;           /**< CQ_FORMAT_Q16_16_CODE or CQ_FORMAT_Q8_24_CODE */
    uint8_t tool_version[4];        /**< Tool version bytes */

    /* Validation */
    cq_fault_flags_t faults;

    uint8_t _reserved[3];
} cq_certificate_builder_t;

/* ============================================================================
 * Builder Interface
 * ============================================================================ */

/**
 * @brief Initialise certificate builder.
 *
 * @param builder  Builder context to initialise.
 */
void cq_certificate_builder_init(cq_certificate_builder_t *builder);

/**
 * @brief Set tool version in builder.
 *
 * @param builder  Builder context.
 * @param major    Major version.
 * @param minor    Minor version.
 * @param patch    Patch version.
 * @param build    Build number.
 */
void cq_certificate_builder_set_version(cq_certificate_builder_t *builder,
                                        uint8_t major,
                                        uint8_t minor,
                                        uint8_t patch,
                                        uint8_t build);

/**
 * @brief Set source model hash.
 *
 * @param builder  Builder context.
 * @param hash     SHA-256 hash of source FP32 model (32 bytes).
 */
void cq_certificate_builder_set_source_hash(cq_certificate_builder_t *builder,
                                            const uint8_t hash[32]);

/**
 * @brief Set BatchNorm folding information.
 *
 * @param builder  Builder context.
 * @param folded   True if BN was folded.
 * @param hash     SHA-256 hash of BN folding record (32 bytes), or NULL if no BN.
 */
void cq_certificate_builder_set_bn_info(cq_certificate_builder_t *builder,
                                        bool folded,
                                        const uint8_t hash[32]);

/**
 * @brief Set analysis digest.
 *
 * @param builder  Builder context.
 * @param digest   Analysis digest from cq_analysis_digest_generate().
 */
void cq_certificate_builder_set_analysis(cq_certificate_builder_t *builder,
                                         const cq_analysis_digest_t *digest);

/**
 * @brief Set calibration digest.
 *
 * @param builder  Builder context.
 * @param digest   Calibration digest from cq_calibration_digest_generate().
 */
void cq_certificate_builder_set_calibration(cq_certificate_builder_t *builder,
                                            const cq_calibration_digest_t *digest);

/**
 * @brief Set verification digest.
 *
 * @param builder  Builder context.
 * @param digest   Verification digest from cq_verification_digest_generate().
 */
void cq_certificate_builder_set_verification(cq_certificate_builder_t *builder,
                                             const cq_verification_digest_t *digest);

/**
 * @brief Set target model information.
 *
 * @param builder      Builder context.
 * @param hash         SHA-256 hash of quantized model (32 bytes).
 * @param param_count  Total parameter count.
 * @param layer_count  Number of layers.
 */
void cq_certificate_builder_set_target(cq_certificate_builder_t *builder,
                                       const uint8_t hash[32],
                                       uint32_t param_count,
                                       uint32_t layer_count);

/**
 * @brief Set quantization format scope.
 *
 * @param builder  Builder context.
 * @param format   CQ_FORMAT_Q16_16_CODE or CQ_FORMAT_Q8_24_CODE.
 */
void cq_certificate_builder_set_format(cq_certificate_builder_t *builder,
                                       uint8_t format);

/**
 * @brief Check if builder has all required inputs.
 *
 * @param builder  Builder context.
 * @return         true if all required fields are set.
 */
bool cq_certificate_builder_is_complete(const cq_certificate_builder_t *builder);

/* ============================================================================
 * Certificate Generation
 * ============================================================================ */

/**
 * @brief Build certificate from accumulated inputs.
 *
 * @param builder  Completed builder context.
 * @param cert     Output: Certificate structure.
 * @param faults   Output: Fault flags.
 * @return         0 on success, negative error code on failure.
 *
 * @pre  cq_certificate_builder_is_complete(builder) == true
 * @post cert->merkle_root is computed
 * @post cert->signature is zeroed (unsigned)
 *
 * @traceability SRS-005-CERTIFICATE
 */
int cq_certificate_build(const cq_certificate_builder_t *builder,
                         cq_certificate_t *cert,
                         cq_fault_flags_t *faults);

/**
 * @brief Compute Merkle root for certificate.
 *
 * @param cert      Certificate (merkle_root field will be computed).
 * @param out_hash  Output buffer for computed hash (32 bytes).
 *
 * @traceability CQ-MATH-001 §9.2
 */
void cq_certificate_compute_merkle(const cq_certificate_t *cert,
                                   uint8_t out_hash[32]);

/* ============================================================================
 * Certificate Verification
 * ============================================================================ */

/**
 * @brief Verify certificate integrity (Merkle root).
 *
 * @param cert  Certificate to verify.
 * @return      true if Merkle root is valid.
 *
 * @traceability SRS-005-CERTIFICATE, CQ-STRUCT-001 §7.2
 */
bool cq_certificate_verify_integrity(const cq_certificate_t *cert);

/**
 * @brief Verify certificate magic and version.
 *
 * @param cert  Certificate to verify.
 * @return      true if magic and version are valid.
 */
bool cq_certificate_verify_header(const cq_certificate_t *cert);

/**
 * @brief Check if certificate claims verification passed.
 *
 * @param cert  Certificate to check.
 * @return      true if measured error ≤ theoretical bound.
 */
static inline bool cq_certificate_bounds_satisfied(const cq_certificate_t *cert) {
    return cert->epsilon_max_measured <= cert->epsilon_total_claimed;
}

/* ============================================================================
 * Serialisation
 * ============================================================================ */

/**
 * @brief Serialise certificate to byte buffer.
 *
 * @param cert    Certificate to serialise.
 * @param buffer  Output buffer (must be CQ_CERTIFICATE_SIZE bytes).
 * @param size    Output: Actual bytes written.
 * @return        0 on success, negative error code on failure.
 */
int cq_certificate_serialise(const cq_certificate_t *cert,
                             uint8_t *buffer,
                             size_t *size);

/**
 * @brief Deserialise certificate from byte buffer.
 *
 * @param buffer  Input buffer (must be CQ_CERTIFICATE_SIZE bytes).
 * @param size    Buffer size.
 * @param cert    Output: Deserialised certificate.
 * @return        0 on success, negative error code on failure.
 */
int cq_certificate_deserialise(const uint8_t *buffer,
                               size_t size,
                               cq_certificate_t *cert);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get current Unix timestamp.
 *
 * @return  Current time as Unix timestamp (seconds since epoch).
 */
uint64_t cq_get_timestamp(void);

/**
 * @brief Format certificate as human-readable string.
 *
 * @param cert    Certificate to format.
 * @param buffer  Output buffer.
 * @param size    Buffer size.
 * @return        Number of characters written (excluding null terminator).
 */
int cq_certificate_format(const cq_certificate_t *cert,
                          char *buffer,
                          size_t size);

#ifdef __cplusplus
}
#endif

#endif /* CQ_CERTIFICATE_H */
