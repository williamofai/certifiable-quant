/**
 * @file sha256.h
 * @project Certifiable-Quant
 * @brief SHA-256 cryptographic hash for audit trails.
 *
 * @traceability CQ-MATH-001 §9, SRS-005-CERTIFICATE
 * @compliance MISRA-C:2012
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#ifndef CQ_SHA256_H
#define CQ_SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CQ_SHA256_BLOCK_SIZE  64
#define CQ_SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[CQ_SHA256_BLOCK_SIZE];
} cq_sha256_ctx_t;

/**
 * @brief Initialize SHA-256 context.
 */
void cq_sha256_init(cq_sha256_ctx_t *ctx);

/**
 * @brief Update hash with data.
 */
void cq_sha256_update(cq_sha256_ctx_t *ctx, const void *data, size_t len);

/**
 * @brief Finalize and output digest.
 */
void cq_sha256_final(cq_sha256_ctx_t *ctx, uint8_t digest[CQ_SHA256_DIGEST_SIZE]);

/**
 * @brief One-shot hash computation.
 */
void cq_sha256(const void *data, size_t len, uint8_t digest[CQ_SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* CQ_SHA256_H */
