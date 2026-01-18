# SRS-001-ANALYZE: Analysis Module Requirements

**Project:** Certifiable-Quant  
**Document ID:** SRS-001-ANALYZE  
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
- Parent: CQ-MATH-001 §3 (Core Mathematical Invariants)
- Data Structures: CQ-STRUCT-001 §3 (Analysis Structures)
- Siblings: SRS-002-CALIBRATE, SRS-003-CONVERT, SRS-004-VERIFY, SRS-005-CERTIFICATE

---

## §1 Purpose

The Analysis Module ("The Theorist") computes theoretical error bounds for a neural network model **without executing inference**. It performs static analysis on the FP32 model graph to populate the `cq_analysis_ctx_t` structure, establishing the mathematical foundation for the entire quantization certificate.

**Core Responsibility:** Prove, before any data flows, that the quantized model will satisfy bounded error guarantees.

---

## §2 Scope

### §2.1 In Scope

- Range propagation through layer graph
- Accumulator overflow proof generation
- Operator norm (amplification factor) computation
- Error recurrence calculation
- Entry error determination
- Total error bound computation

### §2.2 Out of Scope

- Actual inference execution (delegated to SRS-004-VERIFY)
- Calibration data collection (delegated to SRS-002-CALIBRATE)
- Weight conversion (delegated to SRS-003-CONVERT)

---

## §3 Functional Requirements

### FR-ANA-01: Range Propagation

**Requirement ID:** FR-ANA-01  
**Title:** Interval Arithmetic Range Propagation  
**Priority:** Critical

**SHALL:** The system shall compute the theoretical output range of every layer using interval arithmetic based on the `input_spec` and weight ranges.

**Rationale:** Range bounds are required to:
1. Determine safe accumulator sizes (FR-ANA-02)
2. Validate calibration coverage (SRS-002-CALIBRATE)
3. Compute worst-case error amplification

**Method:**

For input range [x_min, x_max] and weight range [w_min, w_max]:

```
Linear layer output range:
  y_min = min(w_min·x_min, w_min·x_max, w_max·x_min, w_max·x_max) · n + b_min
  y_max = max(w_min·x_min, w_min·x_max, w_max·x_min, w_max·x_max) · n + b_max

Where n = number of input connections (fan-in)
```

**Verification:** Test  
**Traceability:** CQ-MATH-001 §3.4, CQ-STRUCT-001 §3.1

---

### FR-ANA-02: Overflow Proof Generation

**Requirement ID:** FR-ANA-02  
**Title:** Accumulator Overflow Safety Proof  
**Priority:** Critical

**SHALL:** The system shall populate `cq_overflow_proof_t` for each layer by computing:

```c
proof.max_weight_mag = max(|w_int|) for all weights in layer
proof.max_input_mag  = max(|x_int|) for all possible inputs
proof.dot_product_len = fan_in (number of multiply-accumulate operations)
proof.safety_margin  = (2^63 - 1) - (n × w_max × x_max)
proof.is_safe = (n × w_max × x_max) < 2^63
```

**Constraint:** If the safety condition is violated (`is_safe = false`), the system shall:
1. Flag the layer for chunked accumulation, OR
2. Flag the layer for mixed precision (Q8.24), OR
3. Halt with `CQ_FAULT_OVERFLOW` if no mitigation is available

**Verification:** Analysis + Test  
**Traceability:** CQ-MATH-001 §3.4, CQ-STRUCT-001 §3.1 (ST-003-A)

---

### FR-ANA-03: Operator Norm Computation

**Requirement ID:** FR-ANA-03  
**Title:** Amplification Factor Calculation  
**Priority:** Critical

**SHALL:** The system shall calculate the operator norm upper bound (A_l) for each layer and store it in `cq_layer_contract_t.amp_factor`.

**Method:** Frobenius norm as upper bound:

```
A_l ≤ ‖W‖_F = √(Σᵢⱼ wᵢⱼ²)

Where ‖W‖_F ≥ ‖W‖_2 (spectral norm)
```

**Implementation:**

```c
double cq_frobenius_norm(const float *W, size_t rows, size_t cols) {
    double sum_sq = 0.0;
    for (size_t i = 0; i < rows * cols; i++) {
        sum_sq += (double)W[i] * (double)W[i];
    }
    return sqrt(sum_sq);
}
```

**Constraint:** `amp_factor` must be ≥ 1.0 (identity has norm 1).

**Verification:** Test (compare against known matrices)  
**Traceability:** CQ-MATH-001 §3.6.2, CQ-STRUCT-001 §3.2 (ST-003-B)

---

### FR-ANA-04: Error Recurrence Computation

**Requirement ID:** FR-ANA-04  
**Title:** Closed-Form Error Bound Propagation  
**Priority:** Critical

**SHALL:** The system shall compute `output_error_bound` (ε_{l+1}) for each layer using the closed-form recurrence from CQ-MATH-001 §3.6.2:

```
ε_{l+1} ≤ A_l·ε_l + ‖ΔW_l‖·‖x_l‖ + ‖Δb_l‖ + ε_proj,l
```

**Field Mapping:**

| Mathematical Term | Struct Field | Computation |
|-------------------|--------------|-------------|
| A_l | `amp_factor` | FR-ANA-03 |
| ε_l | `input_error_bound` | Previous layer output |
| ‖ΔW_l‖·‖x_l‖ | `weight_error_contrib` | (1/2S_w) × ‖x‖_max |
| ‖Δb_l‖ | `bias_error_contrib` | 1/(2·S_acc) |
| ε_proj,l | `projection_error` | 1/(2·S_out) |

**Algorithm:**

```c
void cq_compute_layer_error(cq_layer_contract_t *layer,
                            const cq_layer_contract_t *prev,
                            const cq_tensor_spec_t *specs) {
    // Input error from previous layer (or entry error for layer 0)
    layer->input_error_bound = (prev != NULL) ? prev->output_error_bound
                                               : ctx->entry_error;

    // Static error terms
    double S_w = (double)(1 << specs->weight_spec.scale_exp);
    double S_out = (double)(1 << specs->output_spec.scale_exp);

    layer->weight_error_contrib = (0.5 / S_w) * layer->max_input_norm;
    layer->bias_error_contrib = 0.5 / (S_w * layer->max_input_scale);
    layer->projection_error = 0.5 / S_out;

    layer->local_error_sum = layer->weight_error_contrib +
                             layer->bias_error_contrib +
                             layer->projection_error;

    // Recurrence
    layer->output_error_bound = layer->amp_factor * layer->input_error_bound +
                                layer->local_error_sum;
}
```

**Verification:** Test (verify against analytical solutions for simple networks)  
**Traceability:** CQ-MATH-001 §3.6.2, CQ-STRUCT-001 §3.2 (ST-003-B)

---

### FR-ANA-05: Entry Error Determination

**Requirement ID:** FR-ANA-05  
**Title:** Input Ingress Quantization Error  
**Priority:** Critical

**SHALL:** The system shall compute the entry error ε₀ and store it in `cq_analysis_ctx_t.entry_error`.

**Definition (CQ-MATH-001 §3.6.1):**

```
ε₀ ≤ 1/(2·S_in)

Where S_in = 2^(input_scale_exp)
```

**Special Cases:**

| Input Type | Entry Error |
|------------|-------------|
| FP32 quantized to Q16.16 | 2^(-17) |
| INT8 exactly mapped | 0 (if exact) |
| Pre-quantized input | Inherited from source |

**Verification:** Analysis  
**Traceability:** CQ-MATH-001 §3.6.1, CQ-STRUCT-001 §3.3 (ST-003-C)

---

### FR-ANA-06: Total Error Bound Computation

**Requirement ID:** FR-ANA-06  
**Title:** End-to-End Error Bound  
**Priority:** Critical

**SHALL:** The system shall compute the total error bound by propagating the recurrence through all L layers:

```
ε_total = ε_L (output error of final layer)
```

**SHALL:** Store result in `cq_analysis_ctx_t.total_error_bound`.

**Post-condition:** `total_error_bound > entry_error` (error can only increase through layers).

**Verification:** Test  
**Traceability:** CQ-MATH-001 §3.6.3, CQ-STRUCT-001 §3.3 (ST-003-C)

---

### FR-ANA-07: Model Validation

**Requirement ID:** FR-ANA-07  
**Title:** Pre-Analysis Model Checks  
**Priority:** High

**SHALL:** Before analysis, the system shall validate:

1. **Symmetric Scope Lock:** All quantization parameters have `zero_point = 0`
   - Fail: `CQ_FAULT_ASYMMETRIC_PARAMS`

2. **BatchNorm Folding:** No unfolded BatchNorm layers present
   - Fail: `CQ_FAULT_UNFOLDED_BATCHNORM`

3. **Supported Layer Types:** All layers are in the supported set
   - Supported: Linear, Conv2D, ReLU, MaxPool, AvgPool, Softmax
   - Fail: Return unsupported layer error

**Verification:** Test  
**Traceability:** CQ-MATH-001 §2.5, §8.1

---

## §4 Non-Functional Requirements

### NFR-ANA-01: Determinism

**SHALL:** Analysis results shall be bit-identical across platforms for the same input model.

**Rationale:** Analysis digest is hashed into the certificate.

### NFR-ANA-02: No Dynamic Allocation

**SHALL:** The Analysis module shall not use `malloc()`/`free()` after initialisation.

**Rationale:** Static allocation enables WCET analysis for safety-critical deployment.

### NFR-ANA-03: Complexity Bounds

**SHALL:** Analysis shall complete in O(P) time where P is the total parameter count.

**Rationale:** Predictable execution time for certification.

---

## §5 Interface Specification

### §5.1 Input Interface

```c
/**
 * @brief Analyse FP32 model and populate analysis context.
 * @param model     Pointer to parsed FP32 model graph.
 * @param config    Analysis configuration (input specs, precision targets).
 * @param ctx       Output: Analysis context (caller-allocated).
 * @param faults    Output: Accumulated fault flags.
 * @return          0 on success, negative error code on failure.
 *
 * @pre  model is a valid, BN-folded model graph
 * @pre  ctx->layers array is allocated with ctx->layer_count entries
 * @post ctx->is_complete == true if all layers analysed
 * @post ctx->total_error_bound contains end-to-end bound
 */
int cq_analyze(const cq_model_fp32_t *model,
               const cq_analyze_config_t *config,
               cq_analysis_ctx_t *ctx,
               cq_fault_flags_t *faults);
```

### §5.2 Output Interface

```c
/**
 * @brief Generate analysis digest for certificate.
 * @param ctx       Completed analysis context.
 * @param digest    Output: Analysis digest (fixed-size).
 * @return          0 on success, negative error code on failure.
 *
 * @pre  ctx->is_complete == true
 * @pre  ctx->is_valid == true (no fatal faults)
 */
int cq_analysis_digest_generate(const cq_analysis_ctx_t *ctx,
                                 cq_analysis_digest_t *digest);
```

---

## §6 Error Handling

| Condition | Fault Flag | Action |
|-----------|------------|--------|
| Overflow unsafe, no mitigation | `overflow = 1` | Continue with warning |
| Asymmetric quantization detected | `asymmetric = 1` | Fail closed |
| Unfolded BatchNorm detected | `unfolded_bn = 1` | Fail closed |
| Unsupported layer type | — | Return error code |
| Invalid model graph | — | Return error code |

---

## §7 Traceability Matrix

| Requirement | CQ-MATH-001 | CQ-STRUCT-001 | Test Case |
|-------------|-------------|---------------|-----------|
| FR-ANA-01 | §3.4 | §3.1 | TC-ANA-01 |
| FR-ANA-02 | §3.4 | §3.1 | TC-ANA-02 |
| FR-ANA-03 | §3.6.2 | §3.2 | TC-ANA-03 |
| FR-ANA-04 | §3.6.2 | §3.2 | TC-ANA-04 |
| FR-ANA-05 | §3.6.1 | §3.3 | TC-ANA-05 |
| FR-ANA-06 | §3.6.3 | §3.3 | TC-ANA-06 |
| FR-ANA-07 | §2.5, §8.1 | §2.1 | TC-ANA-07 |

---

## §8 Acceptance Criteria

| Criterion | Metric | Target |
|-----------|--------|--------|
| Range bounds correct | Verified against interval arithmetic reference | 100% match |
| Overflow proofs valid | No false negatives (unsafe marked safe) | 0 false negatives |
| Norm computation | Within 1% of analytical solution | < 1% error |
| Error recurrence | Matches hand calculation for test networks | Exact match |
| Determinism | Bit-identical across x86-64, ARM64 | 100% identical |

---

## Document Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Author | William Murray | | |
| Technical Review | | | |
| Quality Assurance | | | |

---

**Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.**
