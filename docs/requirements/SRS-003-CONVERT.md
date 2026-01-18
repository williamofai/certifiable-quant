# SRS-003-CONVERT: Conversion Module Requirements

**Project:** Certifiable-Quant  
**Document ID:** SRS-003-CONVERT  
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
- Parent: CQ-MATH-001 §2 (Fixed-Point Model), §8 (Preprocessing)
- Data Structures: CQ-STRUCT-001 §5 (Conversion Structures)
- Siblings: SRS-001-ANALYZE, SRS-002-CALIBRATE, SRS-004-VERIFY, SRS-005-CERTIFICATE

---

## §1 Purpose

The Conversion Module ("The Transformer") transforms a validated FP32 model into a fixed-point binary artifact. It produces `cq_model_header_t` and the quantized weight payloads, enforcing all mathematical constraints established in Analysis and validated by Calibration.

**Core Responsibility:** Perform the irreversible transformation from floating-point to fixed-point with full traceability.

---

## §2 Scope

### §2.1 In Scope

- FP32 to Q16.16/Q8.24 weight conversion
- Symmetric quantization enforcement
- Dyadic constraint verification
- BatchNorm folding (if not pre-folded)
- RNE rounding implementation
- Model binary generation
- BN folding audit trail

### §2.2 Out of Scope

- Theoretical bound computation (completed by SRS-001-ANALYZE)
- Calibration statistics (completed by SRS-002-CALIBRATE)
- Output verification (delegated to SRS-004-VERIFY)

---

## §3 Functional Requirements

### FR-CNV-01: Quantization Kernel

**Requirement ID:** FR-CNV-01  
**Title:** FP32 to Fixed-Point Conversion  
**Priority:** Critical

**SHALL:** The system shall convert FP32 weights to fixed-point using the RNE (Round-to-Nearest-Even) rounding mode defined in CQ-MATH-001 §2.3.

**Conversion Formula:**

```
w_q = round_rne(w_fp × 2^n)

Where:
  w_fp = floating-point weight
  n    = fractional bits (16 for Q16.16, 24 for Q8.24)
  w_q  = quantized integer representation
```

**Implementation:**

```c
cq_fixed16_t cq_quantize_q16(float w_fp, cq_fault_flags_t *faults) {
    // Scale to fixed-point domain
    double scaled = (double)w_fp * (double)(1 << 16);
    
    // Round to nearest even
    double rounded = round(scaled);  // C99 round() uses RNE
    
    // Clamp to representable range
    if (rounded > (double)INT32_MAX) {
        faults->overflow = 1;
        return INT32_MAX;
    }
    if (rounded < (double)INT32_MIN) {
        faults->underflow = 1;
        return INT32_MIN;
    }
    
    return (cq_fixed16_t)rounded;
}
```

**Constraint:** Saturation events must set fault flags but conversion continues (saturated values are valid, just flagged).

**Verification:** Test (verify against CQ-MATH-001 §2.3 test vectors)  
**Traceability:** CQ-MATH-001 §2.2, §2.3, CQ-STRUCT-001 §1.1

---

### FR-CNV-02: Symmetric Enforcement

**Requirement ID:** FR-CNV-02  
**Title:** Zero-Point Constraint Verification  
**Priority:** Critical

**SHALL:** The system shall verify that all quantization uses symmetric mode (zero_point = 0).

**SHALL:** If affine/asymmetric parameters are detected:
1. Set `cq_fault_flags_t.asymmetric = 1`
2. Halt conversion with `CQ_FAULT_ASYMMETRIC_PARAMS`

**Implementation:**

```c
int cq_verify_symmetric(const cq_tensor_spec_t *spec,
                        cq_fault_flags_t *faults) {
    if (!spec->is_symmetric) {
        faults->asymmetric = 1;
        return CQ_FAULT_ASYMMETRIC_PARAMS;
    }
    return 0;
}
```

**Rationale:** Asymmetric quantization introduces offset-coupling terms that are out of scope for certified bounds (CQ-MATH-001 §2.5).

**Verification:** Test  
**Traceability:** CQ-MATH-001 §2.5, CQ-STRUCT-001 §5.2 (ST-005-B)

---

### FR-CNV-03: Dyadic Constraint Enforcement

**Requirement ID:** FR-CNV-03  
**Title:** Bias Scale Verification  
**Priority:** Critical

**SHALL:** The system shall verify the dyadic constraint before writing each `cq_layer_header_t`:

```
S_b ≡ S_w × S_x

Equivalently:
bias.scale_exp == weight.scale_exp + input.scale_exp
```

**SHALL:** If constraint is violated:
1. Set `cq_layer_header_t.dyadic_valid = false`
2. Halt conversion with error

**Implementation:**

```c
int cq_verify_dyadic(const cq_layer_header_t *hdr,
                     cq_fault_flags_t *faults) {
    int expected_bias_exp = hdr->weight_spec.scale_exp + 
                            hdr->input_spec.scale_exp;
    
    if (hdr->bias_spec.scale_exp != expected_bias_exp) {
        hdr->dyadic_valid = false;
        return CQ_ERROR_DYADIC_VIOLATION;
    }
    
    hdr->dyadic_valid = true;
    return 0;
}
```

**Rationale:** The dyadic constraint ensures integer addition without runtime rescaling, which would introduce non-determinism.

**Verification:** Test  
**Traceability:** CQ-MATH-001 §3.2, CQ-STRUCT-001 §5.3 (ST-005-C)

---

### FR-CNV-04: BatchNorm Folding

**Requirement ID:** FR-CNV-04  
**Title:** BN Parameter Folding with Audit Trail  
**Priority:** Critical

**SHALL:** If unfolded BatchNorm layers are present, the system shall fold them into preceding linear/conv layers using the formulas from CQ-MATH-001 §8.2:

```
W' = W · γ / √(σ² + ε)
b' = (b - μ) · γ / √(σ² + ε) + β
```

**SHALL:** Before folding, compute SHA-256 of BN parameters and store in `cq_bn_folding_record_t.original_bn_hash`.

**SHALL:** After folding, compute SHA-256 of folded weights and store in `cq_bn_folding_record_t.folded_weights_hash`.

**SHALL:** Folding shall be performed in FP64 precision to minimise error.

**Implementation:**

```c
int cq_fold_batchnorm(const float *W, const float *b,
                      const cq_bn_params_t *bn,
                      float *W_folded, float *b_folded,
                      cq_bn_folding_record_t *record,
                      cq_fault_flags_t *faults) {
    // Hash original BN parameters
    cq_sha256_bn_params(bn, record->original_bn_hash);
    
    // Compute folding factor (FP64 for precision)
    double inv_std = 1.0 / sqrt((double)bn->var + (double)bn->epsilon);
    double scale = (double)bn->gamma * inv_std;
    
    // Fold weights: W' = W × scale
    for (size_t i = 0; i < weight_count; i++) {
        W_folded[i] = (float)((double)W[i] * scale);
    }
    
    // Fold bias: b' = (b - μ) × scale + β
    for (size_t i = 0; i < bias_count; i++) {
        double folded = ((double)b[i] - (double)bn->mean[i]) * scale + 
                        (double)bn->beta[i];
        b_folded[i] = (float)folded;
    }
    
    // Hash folded weights
    cq_sha256_weights(W_folded, b_folded, record->folded_weights_hash);
    record->folding_occurred = true;
    
    return 0;
}
```

**Verification:** Test (verify folded output matches BN(Linear(x)))  
**Traceability:** CQ-MATH-001 §8.2, §8.3, CQ-STRUCT-001 §5.1 (ST-005-A)

---

### FR-CNV-05: Model Binary Generation

**Requirement ID:** FR-CNV-05  
**Title:** Quantized Model File Creation  
**Priority:** Critical

**SHALL:** The system shall generate a binary model file with structure:

```
┌─────────────────────────────────┐
│     cq_model_header_t (§5.4)    │  ← Fixed-size header
├─────────────────────────────────┤
│   cq_layer_header_t[layer_count]│  ← Layer metadata array
├─────────────────────────────────┤
│         Weight Payload 0        │  ← Quantized weights (layer 0)
├─────────────────────────────────┤
│         Bias Payload 0          │  ← Quantized biases (layer 0)
├─────────────────────────────────┤
│              ...                │
├─────────────────────────────────┤
│       Weight Payload N-1        │
├─────────────────────────────────┤
│        Bias Payload N-1         │
└─────────────────────────────────┘
```

**SHALL:** All integers in header shall be little-endian.

**SHALL:** Compute SHA-256 of complete model and store in `cq_model_header_t.quantized_hash`.

**Verification:** Test  
**Traceability:** CQ-MATH-001 §9.2, CQ-STRUCT-001 §5.4 (ST-005-D)

---

### FR-CNV-06: Scale Exponent Selection

**Requirement ID:** FR-CNV-06  
**Title:** Optimal Scale Factor Determination  
**Priority:** High

**SHALL:** The system shall select scale exponents to maximise dynamic range while avoiding overflow:

```
scale_exp = floor(log2(max_representable / max_observed))

Where:
  max_representable = 2^(integer_bits - 1) - 1
  max_observed      = from calibration statistics
```

**For Q16.16:** `max_representable = 32767`  
**For Q8.24:** `max_representable = 127`

**Verification:** Test  
**Traceability:** CQ-MATH-001 §3.1, CQ-STRUCT-001 §5.2

---

### FR-CNV-07: Overflow Mitigation

**Requirement ID:** FR-CNV-07  
**Title:** Unsafe Layer Handling  
**Priority:** High

**SHALL:** For layers where `cq_overflow_proof_t.is_safe == false`:

1. **Option A:** Use Q8.24 format (more integer bits) if sufficient
2. **Option B:** Implement chunked accumulation (split dot product)
3. **Option C:** Halt with error if no mitigation available

**SHALL:** Record mitigation strategy in layer header metadata.

**Verification:** Test  
**Traceability:** CQ-MATH-001 §3.4, CQ-STRUCT-001 §3.1

---

## §4 Non-Functional Requirements

### NFR-CNV-01: Determinism

**SHALL:** Conversion shall produce bit-identical output for identical inputs across all platforms.

**Rationale:** Model hash is included in certificate.

### NFR-CNV-02: Traceability

**SHALL:** Every weight in the output model shall be traceable to its FP32 source via the scale exponent and rounding mode.

### NFR-CNV-03: Atomicity

**SHALL:** Conversion shall be atomic: either complete success or no output file.

**Rationale:** Partial model files are dangerous in safety-critical systems.

---

## §5 Interface Specification

### §5.1 Conversion Interface

```c
/**
 * @brief Convert FP32 model to quantized format.
 * @param model_fp32  Input FP32 model.
 * @param analysis    Completed analysis context.
 * @param calibration Completed calibration report.
 * @param config      Conversion configuration.
 * @param output_path Path for output model file.
 * @param model_out   Output: Model header (for certificate).
 * @param faults      Output: Accumulated fault flags.
 * @return            0 on success, negative error code on failure.
 *
 * @pre  analysis->is_complete == true
 * @pre  calibration->range_veto_triggered == false
 * @post Output file contains valid quantized model
 * @post model_out contains header with hashes
 */
int cq_convert(const cq_model_fp32_t *model_fp32,
               const cq_analysis_ctx_t *analysis,
               const cq_calibration_report_t *calibration,
               const cq_convert_config_t *config,
               const char *output_path,
               cq_model_header_t *model_out,
               cq_fault_flags_t *faults);
```

### §5.2 BN Folding Interface

```c
/**
 * @brief Pre-process model by folding BatchNorm layers.
 * @param model_in    Input model (may have BN layers).
 * @param model_out   Output model (BN-folded).
 * @param records     Output: Array of folding records.
 * @param record_count Output: Number of records.
 * @param faults      Output: Accumulated fault flags.
 * @return            0 on success, negative error code on failure.
 */
int cq_fold_all_batchnorm(const cq_model_fp32_t *model_in,
                          cq_model_fp32_t *model_out,
                          cq_bn_folding_record_t *records,
                          size_t *record_count,
                          cq_fault_flags_t *faults);
```

---

## §6 Error Handling

| Condition | Fault Flag | Action |
|-----------|------------|--------|
| Asymmetric quantization | `asymmetric = 1` | Fail closed |
| Dyadic constraint violation | — | Fail closed |
| Weight overflow | `overflow = 1` | Saturate, continue |
| Weight underflow | `underflow = 1` | Saturate, continue |
| Unsafe overflow, no mitigation | `overflow = 1` | Fail closed |
| File write error | — | Fail closed, cleanup |

---

## §7 Traceability Matrix

| Requirement | CQ-MATH-001 | CQ-STRUCT-001 | Test Case |
|-------------|-------------|---------------|-----------|
| FR-CNV-01 | §2.2, §2.3 | §1.1 | TC-CNV-01 |
| FR-CNV-02 | §2.5 | §5.2 | TC-CNV-02 |
| FR-CNV-03 | §3.2 | §5.3 | TC-CNV-03 |
| FR-CNV-04 | §8.2, §8.3 | §5.1 | TC-CNV-04 |
| FR-CNV-05 | §9.2 | §5.4 | TC-CNV-05 |
| FR-CNV-06 | §3.1 | §5.2 | TC-CNV-06 |
| FR-CNV-07 | §3.4 | §3.1 | TC-CNV-07 |

---

## §8 Acceptance Criteria

| Criterion | Metric | Target |
|-----------|--------|--------|
| RNE rounding | Matches test vectors | 100% match |
| Symmetric enforcement | No asymmetric models pass | 0 false accepts |
| Dyadic verification | No violations pass | 0 false accepts |
| BN folding | Output matches BN(Linear(x)) | < 1e-6 error |
| Model hash | Reproducible across platforms | Bit-identical |
| Determinism | Identical outputs across runs | 100% identical |

---

## Document Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Author | William Murray | | |
| Technical Review | | | |
| Quality Assurance | | | |

---

**Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.**
