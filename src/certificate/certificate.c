/**
 * @file certificate.c
 * @project Certifiable-Quant
 * @brief Certificate module implementation (The Notary)
 *
 * @traceability SRS-005-CERTIFICATE, CQ-MATH-001 §9
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * @license GPL-3.0 or Commercial (contact: william@fstopify.com)
 */

#include "certificate.h"
#include "sha256.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ============================================================================
 * Builder Interface
 * ============================================================================ */

void cq_certificate_builder_init(cq_certificate_builder_t *builder)
{
    if (builder == NULL) {
        return;
    }

    memset(builder, 0, sizeof(*builder));

    /* Default format: Q16.16 */
    builder->scope_format = CQ_FORMAT_Q16_16_CODE;

    /* Default version: 0.1.0.0 */
    builder->tool_version[0] = 0;
    builder->tool_version[1] = 1;
    builder->tool_version[2] = 0;
    builder->tool_version[3] = 0;

    cq_fault_clear(&builder->faults);
}

void cq_certificate_builder_set_version(cq_certificate_builder_t *builder,
                                        uint8_t major,
                                        uint8_t minor,
                                        uint8_t patch,
                                        uint8_t build)
{
    if (builder == NULL) {
        return;
    }

    builder->tool_version[0] = major;
    builder->tool_version[1] = minor;
    builder->tool_version[2] = patch;
    builder->tool_version[3] = build;
}

void cq_certificate_builder_set_source_hash(cq_certificate_builder_t *builder,
                                            const uint8_t hash[32])
{
    if (builder == NULL || hash == NULL) {
        return;
    }

    memcpy(builder->source_model_hash, hash, 32);
    builder->source_model_hash_set = true;
}

void cq_certificate_builder_set_bn_info(cq_certificate_builder_t *builder,
                                        bool folded,
                                        const uint8_t hash[32])
{
    if (builder == NULL) {
        return;
    }

    builder->bn_folded = folded;

    if (hash != NULL) {
        memcpy(builder->bn_folding_hash, hash, 32);
    } else {
        memset(builder->bn_folding_hash, 0, 32);
    }

    builder->bn_info_set = true;
}

void cq_certificate_builder_set_analysis(cq_certificate_builder_t *builder,
                                         const cq_analysis_digest_t *digest)
{
    if (builder == NULL || digest == NULL) {
        return;
    }

    memcpy(&builder->analysis_digest, digest, sizeof(cq_analysis_digest_t));
    builder->analysis_set = true;
}

void cq_certificate_builder_set_calibration(cq_certificate_builder_t *builder,
                                            const cq_calibration_digest_t *digest)
{
    if (builder == NULL || digest == NULL) {
        return;
    }

    memcpy(&builder->calibration_digest, digest, sizeof(cq_calibration_digest_t));
    builder->calibration_set = true;
}

void cq_certificate_builder_set_verification(cq_certificate_builder_t *builder,
                                             const cq_verification_digest_t *digest)
{
    if (builder == NULL || digest == NULL) {
        return;
    }

    memcpy(&builder->verification_digest, digest, sizeof(cq_verification_digest_t));
    builder->verification_set = true;
}

void cq_certificate_builder_set_target(cq_certificate_builder_t *builder,
                                       const uint8_t hash[32],
                                       uint32_t param_count,
                                       uint32_t layer_count)
{
    if (builder == NULL || hash == NULL) {
        return;
    }

    memcpy(builder->target_model_hash, hash, 32);
    builder->target_param_count = param_count;
    builder->target_layer_count = layer_count;
    builder->target_set = true;
}

void cq_certificate_builder_set_format(cq_certificate_builder_t *builder,
                                       uint8_t format)
{
    if (builder == NULL) {
        return;
    }

    builder->scope_format = format;
}

bool cq_certificate_builder_is_complete(const cq_certificate_builder_t *builder)
{
    if (builder == NULL) {
        return false;
    }

    return builder->source_model_hash_set &&
           builder->bn_info_set &&
           builder->analysis_set &&
           builder->calibration_set &&
           builder->verification_set &&
           builder->target_set;
}

/* ============================================================================
 * Certificate Generation
 * ============================================================================ */

/**
 * @brief Hash a digest structure to 32 bytes.
 */
static void hash_analysis_digest(const cq_analysis_digest_t *digest,
                                 uint8_t out[32])
{
    cq_sha256_ctx_t ctx;
    cq_sha256_init(&ctx);
    cq_sha256_update(&ctx, (const uint8_t *)digest, sizeof(cq_analysis_digest_t));
    cq_sha256_final(&ctx, out);
}

static void hash_calibration_digest(const cq_calibration_digest_t *digest,
                                    uint8_t out[32])
{
    cq_sha256_ctx_t ctx;
    cq_sha256_init(&ctx);
    cq_sha256_update(&ctx, (const uint8_t *)digest, sizeof(cq_calibration_digest_t));
    cq_sha256_final(&ctx, out);
}

static void hash_verification_digest(const cq_verification_digest_t *digest,
                                     uint8_t out[32])
{
    cq_sha256_ctx_t ctx;
    cq_sha256_init(&ctx);
    cq_sha256_update(&ctx, (const uint8_t *)digest, sizeof(cq_verification_digest_t));
    cq_sha256_final(&ctx, out);
}

int cq_certificate_build(const cq_certificate_builder_t *builder,
                         cq_certificate_t *cert,
                         cq_fault_flags_t *faults)
{
    if (builder == NULL || cert == NULL || faults == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    if (!cq_certificate_builder_is_complete(builder)) {
        return -2;  /* Incomplete builder */
    }

    memset(cert, 0, sizeof(*cert));

    /* 1. Metadata Header */
    memcpy(cert->magic, CQ_CERTIFICATE_MAGIC, 4);
    memcpy(cert->version, builder->tool_version, 4);
    cert->timestamp = cq_get_timestamp();

    /* 2. Scope Declaration */
    cert->scope_symmetric_only = CQ_SCOPE_SYMMETRIC_ONLY;
    cert->scope_format = builder->scope_format;

    /* 3. Source Identity */
    memcpy(cert->source_model_hash, builder->source_model_hash, 32);
    memcpy(cert->bn_folding_hash, builder->bn_folding_hash, 32);
    cert->bn_folding_status = builder->bn_folded ? 0x01 : 0x00;

    /* 4. Mathematical Core - hash the digests */
    hash_analysis_digest(&builder->analysis_digest, cert->analysis_digest);
    hash_calibration_digest(&builder->calibration_digest, cert->calibration_digest);
    hash_verification_digest(&builder->verification_digest, cert->verification_digest);

    /* 5. Claims */
    cert->epsilon_0_claimed = builder->analysis_digest.entry_error;
    cert->epsilon_total_claimed = builder->analysis_digest.total_error_bound;
    cert->epsilon_max_measured = builder->verification_digest.total_error_max_measured;

    /* 6. Target Identity */
    memcpy(cert->target_model_hash, builder->target_model_hash, 32);
    cert->target_param_count = builder->target_param_count;
    cert->target_layer_count = builder->target_layer_count;

    /* 7. Integrity - compute Merkle root */
    cq_certificate_compute_merkle(cert, cert->merkle_root);

    /* Signature left as zeros (unsigned) */
    memset(cert->signature, 0, 64);

    /* Copy faults from builder */
    cq_fault_merge(faults, &builder->faults);

    return 0;
}

void cq_certificate_compute_merkle(const cq_certificate_t *cert,
                                   uint8_t out_hash[32])
{
    if (cert == NULL || out_hash == NULL) {
        return;
    }

    /*
     * Merkle root computation:
     * Hash all sections except merkle_root and signature.
     * This includes bytes 0 through 263 (264 bytes total before integrity section).
     */
    cq_sha256_ctx_t ctx;
    cq_sha256_init(&ctx);

    /* Hash everything before the integrity section */
    /* Sections 1-6: 264 bytes */
    const size_t content_size = offsetof(cq_certificate_t, merkle_root);
    cq_sha256_update(&ctx, (const uint8_t *)cert, content_size);

    cq_sha256_final(&ctx, out_hash);
}

/* ============================================================================
 * Certificate Verification
 * ============================================================================ */

bool cq_certificate_verify_integrity(const cq_certificate_t *cert)
{
    if (cert == NULL) {
        return false;
    }

    uint8_t computed_root[32];
    cq_certificate_compute_merkle(cert, computed_root);

    return memcmp(computed_root, cert->merkle_root, 32) == 0;
}

bool cq_certificate_verify_header(const cq_certificate_t *cert)
{
    if (cert == NULL) {
        return false;
    }

    /* Check magic */
    if (memcmp(cert->magic, CQ_CERTIFICATE_MAGIC, 4) != 0) {
        return false;
    }

    /* Check scope is symmetric only */
    if (cert->scope_symmetric_only != CQ_SCOPE_SYMMETRIC_ONLY) {
        return false;
    }

    /* Check format is valid */
    if (cert->scope_format != CQ_FORMAT_Q16_16_CODE &&
        cert->scope_format != CQ_FORMAT_Q8_24_CODE) {
        return false;
    }

    return true;
}

/* ============================================================================
 * Serialisation
 * ============================================================================ */

int cq_certificate_serialise(const cq_certificate_t *cert,
                             uint8_t *buffer,
                             size_t *size)
{
    if (cert == NULL || buffer == NULL || size == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    /* Certificate is already in serialisable form (fixed layout) */
    memcpy(buffer, cert, sizeof(cq_certificate_t));
    *size = sizeof(cq_certificate_t);

    return 0;
}

int cq_certificate_deserialise(const uint8_t *buffer,
                               size_t size,
                               cq_certificate_t *cert)
{
    if (buffer == NULL || cert == NULL) {
        return CQ_ERROR_NULL_POINTER;
    }

    if (size < sizeof(cq_certificate_t)) {
        return -2;  /* Buffer too small */
    }

    memcpy(cert, buffer, sizeof(cq_certificate_t));

    /* Verify header after deserialisation */
    if (!cq_certificate_verify_header(cert)) {
        return -3;  /* Invalid header */
    }

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

uint64_t cq_get_timestamp(void)
{
    return (uint64_t)time(NULL);
}

int cq_certificate_format(const cq_certificate_t *cert,
                          char *buffer,
                          size_t size)
{
    if (cert == NULL || buffer == NULL || size == 0) {
        return 0;
    }

    int written = snprintf(buffer, size,
        "=== CQ Certificate ===\n"
        "Magic: %.4s\n"
        "Version: %d.%d.%d.%d\n"
        "Timestamp: %llu\n"
        "Format: %s\n"
        "BN Folded: %s\n"
        "Entry Error (ε₀): %.6e\n"
        "Total Error (ε_total): %.6e\n"
        "Measured Error (ε_max): %.6e\n"
        "Bounds Satisfied: %s\n"
        "Layers: %u\n"
        "Parameters: %u\n"
        "Integrity: %s\n",
        cert->magic,
        cert->version[0], cert->version[1], cert->version[2], cert->version[3],
        (unsigned long long)cert->timestamp,
        (cert->scope_format == CQ_FORMAT_Q16_16_CODE) ? "Q16.16" : "Q8.24",
        (cert->bn_folding_status == 0x01) ? "Yes" : "No",
        cert->epsilon_0_claimed,
        cert->epsilon_total_claimed,
        cert->epsilon_max_measured,
        cq_certificate_bounds_satisfied(cert) ? "YES" : "NO",
        cert->target_layer_count,
        cert->target_param_count,
        cq_certificate_verify_integrity(cert) ? "VALID" : "INVALID"
    );

    return written;
}
