# SRS-005-CERTIFICATE: Certificate Module Requirements

**Project:** Certifiable-Quant  
**Document ID:** SRS-005-CERTIFICATE  
**Version:** 1.0  
**Status:** Final  
**Date:** January 2026  
**Author:** William Murray  
**Classification:** Software Requirements Specification

---

## Document Control

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | Jan 2026 | W. Murray | Initial release |

**Traceability:**
- Parent: CQ-MATH-001 §9 (Certificate Specification)
- Data Structures: CQ-STRUCT-001 §7 (Certificate Structure)
- Siblings: SRS-001-ANALYZE, SRS-002-CALIBRATE, SRS-003-CONVERT, SRS-004-VERIFY

---

## §1 Purpose

The Certificate Module ("The Notary") assembles and serialises the final proof object that attests to the validity of the quantization process. It combines digests from all previous modules into a Merkle-like structure, creating an immutable, verifiable record.

**Core Responsibility:** Produce an auditable, tamper-evident certificate that binds mathematical proofs to the binary model.

---

## §2 Scope

### §2.1 In Scope

- Canonical serialisation of all digests
- Merkle root computation
- Certificate structure assembly
- Optional Ed25519 signature
- Certificate file generation
- Certificate verification

### §2.2 Out of Scope

- Analysis computation (completed by SRS-001-ANALYZE)
- Calibration (completed by SRS-002-CALIBRATE)
- Conversion (completed by SRS-003-CONVERT)
- Verification (completed by SRS-004-VERIFY)

---

## §3 Functional Requirements

### FR-CRT-01: Canonical Serialisation

**Requirement ID:** FR-CRT-01  
**Title:** Deterministic Byte-Level Serialisation  
**Priority:** Critical

**SHALL:** The system shall serialise all structures to little-endian byte arrays with explicit handling:

| Rule | Requirement |
|------|-------------|
| **Byte order** | Little-endian for all integers |
| **Padding** | Zeroed explicitly (not left uninitialised) |
| **Alignment** | 8-byte aligned structures |
| **Floating-point** | IEEE-754 binary64, little-endian |
| **Booleans** | 0x00 (false) or 0x01 (true), 1 byte |

**Implementation:**

```c
/**
 * @brief Zero padding bytes before serialisation.
 */
void cq_zero_padding(void *ptr, size_t size) {
    memset(ptr, 0, size);
}

/**
 * @brief Serialise uint64_t to little-endian bytes.
 */
void cq_write_u64_le(uint8_t *buf, uint64_t val) {
    buf[0] = (uint8_t)(val >>  0);
    buf[1] = (uint8_t)(val >>  8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);
    buf[4] = (uint8_t)(val >> 32);
    buf[5] = (uint8_t)(val >> 40);
    buf[6] = (uint8_t)(val >> 48);
    buf[7] = (uint8_t)(val >> 56);
}

/**
 * @brief Serialise double to little-endian bytes.
 */
void cq_write_f64_le(uint8_t *buf, double val) {
    uint64_t bits;
    memcpy(&bits, &val, sizeof(bits));
    cq_write_u64_le(buf, bits);
}
```

**Rationale:** Canonical serialisation ensures that identical logical content produces identical byte sequences, which is essential for reproducible hashing.

**Verification:** Test (verify byte sequences match across platforms)  
**Traceability:** CQ-MATH-001 §6 (Determinism), CQ-STRUCT-001 §8.1 (ST-008-A)

---

### FR-CRT-02: Merkle Root Construction

**Requirement ID:** FR-CRT-02  
**Title:** Hash Chain Computation  
**Priority:** Critical

**SHALL:** The system shall compute the `merkle_root` by hashing the concatenation of all section digests in fixed order:

```
merkle_root = SHA-256(
    metadata_bytes      ||  // Sections 1-2
    source_identity     ||  // Section 3
    analysis_digest     ||  // Section 4a
    calibration_digest  ||  // Section 4b
    verification_digest ||  // Section 4c
    claims_bytes        ||  // Section 5
    target_identity         // Section 6
)
```

**Section Layout (from CQ-STRUCT-001 §7.1):**

| Section | Bytes | Content |
|---------|-------|---------|
| 1. Metadata | 16 | magic, version, timestamp |
| 2. Scope | 8 | symmetric_only, format |
| 3. Source | 72 | source_model_hash, bn_folding_hash, status |
| 4. Math Core | 96 | analysis, calibration, verification digests |
| 5. Claims | 32 | ε₀, ε_total, ε_max_measured |
| 6. Target | 40 | target_model_hash, param_count, layer_count |
| **Total** | **264** | Input to Merkle hash |

**Implementation:**

```c
void cq_compute_merkle_root(const cq_certificate_t *cert,
                            uint8_t out_hash[32]) {
    // Serialise sections 1-6 to contiguous buffer
    uint8_t buffer[264];
    size_t offset = 0;
    
    // Section 1: Metadata
    memcpy(buffer + offset, cert->magic, 4);
    offset += 4;
    memcpy(buffer + offset, cert->version, 4);
    offset += 4;
    cq_write_u64_le(buffer + offset, cert->timestamp);
    offset += 8;
    
    // Section 2: Scope
    buffer[offset++] = cert->scope_symmetric_only;
    buffer[offset++] = cert->scope_format;
    memset(buffer + offset, 0, 6);  // Reserved
    offset += 6;
    
    // Section 3: Source identity
    memcpy(buffer + offset, cert->source_model_hash, 32);
    offset += 32;
    memcpy(buffer + offset, cert->bn_folding_hash, 32);
    offset += 32;
    buffer[offset++] = cert->bn_folding_status;
    memset(buffer + offset, 0, 7);  // Reserved
    offset += 7;
    
    // Section 4: Mathematical core
    memcpy(buffer + offset, cert->analysis_digest, 32);
    offset += 32;
    memcpy(buffer + offset, cert->calibration_digest, 32);
    offset += 32;
    memcpy(buffer + offset, cert->verification_digest, 32);
    offset += 32;
    
    // Section 5: Claims
    cq_write_f64_le(buffer + offset, cert->epsilon_0_claimed);
    offset += 8;
    cq_write_f64_le(buffer + offset, cert->epsilon_total_claimed);
    offset += 8;
    cq_write_f64_le(buffer + offset, cert->epsilon_max_measured);
    offset += 8;
    memset(buffer + offset, 0, 8);  // Reserved
    offset += 8;
    
    // Section 6: Target identity
    memcpy(buffer + offset, cert->target_model_hash, 32);
    offset += 32;
    cq_write_u32_le(buffer + offset, cert->target_param_count);
    offset += 4;
    cq_write_u32_le(buffer + offset, cert->target_layer_count);
    offset += 4;
    
    // Compute SHA-256
    cq_sha256(buffer, offset, out_hash);
}
```

**Verification:** Test  
**Traceability:** CQ-MATH-001 §9.2, CQ-STRUCT-001 §7.1 (ST-007-A)

---

### FR-CRT-03: Optional Signature

**Requirement ID:** FR-CRT-03  
**Title:** Ed25519 Digital Signature  
**Priority:** Medium

**SHALL:** If a private key is provided, the system shall sign the `merkle_root` using Ed25519:

```
signature = Ed25519_Sign(private_key, merkle_root)
```

**SHALL:** Store the 64-byte signature in `cq_certificate_t.signature`.

**SHALL:** If no private key is provided, fill signature field with zeros and mark as unsigned.

**Implementation:**

```c
int cq_certificate_sign(cq_certificate_t *cert,
                        const uint8_t private_key[32]) {
    if (private_key == NULL) {
        memset(cert->signature, 0, 64);
        return 0;  // Unsigned certificate
    }
    
    // Sign the merkle root
    return ed25519_sign(cert->signature,
                        cert->merkle_root, 32,
                        private_key);
}
```

**Verification:** Test  
**Traceability:** CQ-STRUCT-001 §7.1 (ST-007-A)

---

### FR-CRT-04: Certificate Assembly

**Requirement ID:** FR-CRT-04  
**Title:** Complete Certificate Generation  
**Priority:** Critical

**SHALL:** The system shall assemble the certificate from all module outputs:

| Source | Destination |
|--------|-------------|
| Tool version | `cert.version` |
| Current UTC time | `cert.timestamp` |
| Analysis digest | `cert.analysis_digest` |
| Calibration digest | `cert.calibration_digest` |
| Verification digest | `cert.verification_digest` |
| `analysis.entry_error` | `cert.epsilon_0_claimed` |
| `analysis.total_error_bound` | `cert.epsilon_total_claimed` |
| `verification.total_error_max_measured` | `cert.epsilon_max_measured` |
| Model header hash | `cert.target_model_hash` |

**Pre-conditions:**

| Check | Fail Action |
|-------|-------------|
| Analysis complete | Abort |
| Calibration passed (no range veto) | Abort |
| Verification passed (bounds satisfied) | Abort |
| No fatal faults | Abort |

**Implementation:**

```c
int cq_certificate_assemble(cq_certificate_t *cert,
                            const cq_analysis_ctx_t *analysis,
                            const cq_calibration_report_t *calibration,
                            const cq_verification_report_t *verification,
                            const cq_model_header_t *model,
                            cq_fault_flags_t *faults) {
    // Pre-condition checks
    if (!analysis->is_complete || !analysis->is_valid) {
        return CQ_ERROR_ANALYSIS_INCOMPLETE;
    }
    if (calibration->range_veto_triggered) {
        return CQ_ERROR_CALIBRATION_VETO;
    }
    if (!verification->all_bounds_satisfied) {
        faults->bound_violation = 1;
        return CQ_ERROR_VERIFICATION_FAILED;
    }
    
    // Zero the certificate first
    memset(cert, 0, sizeof(*cert));
    
    // Metadata
    memcpy(cert->magic, CQ_CERTIFICATE_MAGIC, 4);
    cq_get_tool_version(cert->version);
    cert->timestamp = cq_utc_timestamp();
    
    // Scope
    cert->scope_symmetric_only = 0x01;
    cert->scope_format = CQ_FORMAT_Q16_16;
    
    // Source identity
    memcpy(cert->source_model_hash, model->source_model_hash, 32);
    // ... (BN folding from conversion)
    
    // Mathematical core (hash the digests)
    cq_analysis_digest_t a_digest;
    cq_calibration_digest_t c_digest;
    cq_verification_digest_t v_digest;
    
    cq_analysis_digest_generate(analysis, &a_digest);
    cq_sha256(&a_digest, sizeof(a_digest), cert->analysis_digest);
    
    cq_calibration_digest_generate(calibration, &c_digest);
    cq_sha256(&c_digest, sizeof(c_digest), cert->calibration_digest);
    
    cq_verification_digest_generate(verification, &v_digest);
    cq_sha256(&v_digest, sizeof(v_digest), cert->verification_digest);
    
    // Claims
    cert->epsilon_0_claimed = analysis->entry_error;
    cert->epsilon_total_claimed = analysis->total_error_bound;
    cert->epsilon_max_measured = verification->total_error_max_measured;
    
    // Target identity
    memcpy(cert->target_model_hash, model->quantized_hash, 32);
    cert->target_param_count = model->total_params;
    cert->target_layer_count = model->layer_count;
    
    // Compute Merkle root
    cq_compute_merkle_root(cert, cert->merkle_root);
    
    return 0;
}
```

**Verification:** Test  
**Traceability:** CQ-MATH-001 §9.2, CQ-STRUCT-001 §7.1 (ST-007-A)

---

### FR-CRT-05: Certificate File Output

**Requirement ID:** FR-CRT-05  
**Title:** Binary Certificate Serialisation  
**Priority:** High

**SHALL:** The system shall write the certificate to a binary file with:
- Fixed size: 360 bytes (CQ_CERTIFICATE_SIZE)
- Extension: `.cert`
- Format: Raw binary, no headers

**SHALL:** Compute and verify round-trip: `deserialize(serialize(cert)) == cert`

**Verification:** Test  
**Traceability:** CQ-STRUCT-001 §7.1 (ST-007-A)

---

### FR-CRT-06: Certificate Verification

**Requirement ID:** FR-CRT-06  
**Title:** Independent Certificate Validation  
**Priority:** High

**SHALL:** The system shall provide verification that:

1. **Merkle Root:** Recompute from sections, compare to stored root
2. **Signature:** Verify Ed25519 signature (if present)
3. **Model Binding:** Compare `target_model_hash` to actual model hash
4. **Bounds:** Confirm `epsilon_max_measured ≤ epsilon_total_claimed`

**Implementation:**

```c
typedef struct {
    bool merkle_valid;
    bool signature_valid;
    bool model_binding_valid;
    bool bounds_valid;
} cq_certificate_check_t;

int cq_certificate_verify(const cq_certificate_t *cert,
                          const cq_model_header_t *model,
                          const uint8_t public_key[32],  // NULL if unsigned
                          cq_certificate_check_t *result) {
    // 1. Merkle root verification
    uint8_t computed_root[32];
    cq_compute_merkle_root(cert, computed_root);
    result->merkle_valid = (memcmp(computed_root, 
                                    cert->merkle_root, 32) == 0);
    
    // 2. Signature verification (if applicable)
    if (public_key != NULL && !cq_is_zero(cert->signature, 64)) {
        result->signature_valid = ed25519_verify(
            cert->signature, cert->merkle_root, 32, public_key) == 0;
    } else {
        result->signature_valid = true;  // No signature to verify
    }
    
    // 3. Model binding
    result->model_binding_valid = (memcmp(cert->target_model_hash,
                                          model->quantized_hash, 32) == 0);
    
    // 4. Bounds satisfaction
    result->bounds_valid = (cert->epsilon_max_measured <= 
                           cert->epsilon_total_claimed);
    
    return (result->merkle_valid && result->signature_valid &&
            result->model_binding_valid && result->bounds_valid) ? 0 : -1;
}
```

**Verification:** Test  
**Traceability:** CQ-STRUCT-001 §7.2 (ST-007-B)

---

### FR-CRT-07: Tamper Detection

**Requirement ID:** FR-CRT-07  
**Title:** Hash Chain Integrity  
**Priority:** Critical

**SHALL:** Any modification to any certificate field shall invalidate the Merkle root.

**Demonstration:** Changing a single bit in any digest shall result in:
1. Different Merkle root
2. Signature verification failure (if signed)

**Rationale:** This is the fundamental security property of Merkle trees.

**Verification:** Test (bit-flip tests)  
**Traceability:** CQ-MATH-001 §9

---

## §4 Non-Functional Requirements

### NFR-CRT-01: Determinism

**SHALL:** Certificate generation shall be deterministic except for:
- `timestamp` (intentionally unique)
- `signature` (depends on private key)

**SHALL:** For fixed timestamp and unsigned mode, certificates shall be bit-identical across platforms.

### NFR-CRT-02: Size Constraint

**SHALL:** Certificate size shall be exactly 360 bytes.

**Rationale:** Fixed size enables memory-mapped validation and embedded deployment.

### NFR-CRT-03: Self-Contained

**SHALL:** The certificate shall contain sufficient information to:
1. Identify the source and target models
2. State the error bounds
3. Verify its own integrity

---

## §5 Interface Specification

### §5.1 Generation Interface

```c
/**
 * @brief Generate complete certificate.
 * @param analysis      Completed analysis context.
 * @param calibration   Completed calibration report.
 * @param verification  Completed verification report.
 * @param model         Quantized model header.
 * @param private_key   Ed25519 private key (NULL for unsigned).
 * @param cert          Output: Certificate structure.
 * @param faults        Output: Accumulated fault flags.
 * @return              0 on success, negative error code on failure.
 *
 * @pre  All modules completed successfully
 * @pre  No fatal faults in any module
 * @post cert->merkle_root is valid
 * @post cert->signature is valid (if private_key provided)
 */
int cq_certificate_generate(const cq_analysis_ctx_t *analysis,
                            const cq_calibration_report_t *calibration,
                            const cq_verification_report_t *verification,
                            const cq_model_header_t *model,
                            const uint8_t private_key[32],
                            cq_certificate_t *cert,
                            cq_fault_flags_t *faults);
```

### §5.2 File I/O Interface

```c
/**
 * @brief Write certificate to file.
 */
int cq_certificate_write(const cq_certificate_t *cert,
                         const char *path);

/**
 * @brief Read certificate from file.
 */
int cq_certificate_read(const char *path,
                        cq_certificate_t *cert);
```

### §5.3 Verification Interface

```c
/**
 * @brief Verify certificate integrity and model binding.
 */
int cq_certificate_verify(const cq_certificate_t *cert,
                          const cq_model_header_t *model,
                          const uint8_t public_key[32],
                          cq_certificate_check_t *result);
```

---

## §6 Error Handling

| Condition | Action |
|-----------|--------|
| Analysis incomplete | Abort certificate generation |
| Calibration veto triggered | Abort certificate generation |
| Verification bounds violated | Abort certificate generation |
| Fatal fault in any module | Abort certificate generation |
| Merkle root mismatch | Verification fails |
| Signature invalid | Verification fails |
| Model hash mismatch | Verification fails |

---

## §7 Traceability Matrix

| Requirement | CQ-MATH-001 | CQ-STRUCT-001 | Test Case |
|-------------|-------------|---------------|-----------|
| FR-CRT-01 | §6 | §8.1 | TC-CRT-01 |
| FR-CRT-02 | §9.2 | §7.1 | TC-CRT-02 |
| FR-CRT-03 | — | §7.1 | TC-CRT-03 |
| FR-CRT-04 | §9.2 | §7.1 | TC-CRT-04 |
| FR-CRT-05 | — | §7.1 | TC-CRT-05 |
| FR-CRT-06 | — | §7.2 | TC-CRT-06 |
| FR-CRT-07 | §9 | §7.1 | TC-CRT-07 |

---

## §8 Acceptance Criteria

| Criterion | Metric | Target |
|-----------|--------|--------|
| Serialisation determinism | Bit-identical across platforms | 100% |
| Merkle root computation | Matches reference | Exact match |
| Signature verification | Ed25519 interoperability | Pass standard vectors |
| Round-trip integrity | serialize(deserialize(x)) == x | 100% |
| Tamper detection | Single bit flip detected | 100% detection |
| Size constraint | Exactly 360 bytes | Exact |

---

## §9 Certificate Trust Model

The certificate establishes trust through a chain of cryptographic commitments:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         TRUST HIERARCHY                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────┐                                                        │
│  │  Signature  │  ◄── Ed25519(private_key, merkle_root)                │
│  │    [64]     │      Proves: Certificate issued by key holder          │
│  └──────┬──────┘                                                        │
│         │                                                               │
│         ▼                                                               │
│  ┌─────────────┐                                                        │
│  │ Merkle Root │  ◄── SHA-256(all sections)                            │
│  │    [32]     │      Proves: Certificate content unchanged             │
│  └──────┬──────┘                                                        │
│         │                                                               │
│    ┌────┴────┬────────────┬────────────┐                               │
│    │         │            │            │                               │
│    ▼         ▼            ▼            ▼                               │
│ ┌──────┐ ┌──────┐    ┌──────┐    ┌──────┐                              │
│ │Analy-│ │Calib-│    │Verif-│    │Model │                              │
│ │sis   │ │ration│    │ication│   │Hash  │                              │
│ │Digest│ │Digest│    │Digest│    │ [32] │                              │
│ └──┬───┘ └──┬───┘    └──┬───┘    └──┬───┘                              │
│    │        │           │           │                                   │
│    ▼        ▼           ▼           ▼                                   │
│ ε_total  Coverage    ε_measured   model_q16.bin                         │
│ proofs   metrics     validation   (quantized weights)                   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

VERIFICATION CHAIN:
1. Auditor obtains certificate + model
2. Recompute model hash → compare to cert.target_model_hash
3. Recompute Merkle root → compare to cert.merkle_root
4. Verify signature (if applicable) → proves issuer identity
5. Check ε_max_measured ≤ ε_total_claimed → proves bounds
```

**Trust Guarantees:**

| Property | Mechanism |
|----------|-----------|
| Integrity | Merkle root detects any modification |
| Authenticity | Ed25519 signature proves issuer |
| Non-repudiation | Signed certificate cannot be denied |
| Traceability | Digests link to source artifacts |
| Auditability | All inputs are hashable and verifiable |

---

## Document Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Author | William Murray | | |
| Technical Review | | | |
| Quality Assurance | | | |

---

**Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.**
