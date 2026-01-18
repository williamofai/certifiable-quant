# SRS-004-VERIFY: Verification Module Requirements

**Project:** Certifiable-Quant  
**Document ID:** SRS-004-VERIFY  
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
- Parent: CQ-MATH-001 §7 (Verification Logic)
- Data Structures: CQ-STRUCT-001 §6 (Verification Structures)
- Siblings: SRS-001-ANALYZE, SRS-002-CALIBRATE, SRS-003-CONVERT, SRS-005-CERTIFICATE

---

## §1 Purpose

The Verification Module ("The Judge") validates that the quantized model meets the theoretical error bounds established during Analysis. It runs both FP32 and quantized models on verification data, measures actual deviations, and determines pass/fail status.

**Core Responsibility:** Prove empirically that measured error ≤ theoretical bound, closing the mathematical loop.

---

## §2 Scope

### §2.1 In Scope

- Dual inference execution (FP32 and quantized)
- Per-layer error measurement
- End-to-end error measurement
- Bound satisfaction verification
- Generation of verification digest for certificate
- Falsification of theoretical bounds

### §2.2 Out of Scope

- Theoretical bound computation (completed by SRS-001-ANALYZE)
- Weight quantization (completed by SRS-003-CONVERT)
- Certificate generation (delegated to SRS-005-CERTIFICATE)

---

## §3 Functional Requirements

### FR-VER-01: Dual Inference Execution

**Requirement ID:** FR-VER-01  
**Title:** Parallel FP32 and Quantized Execution  
**Priority:** Critical

**SHALL:** The system shall execute both the FP32 reference model and the quantized model on identical inputs in lockstep.

**Execution Model:**

```
For each verification sample x:
    y_fp = M_fp(x)      // FP32 inference
    y_q  = M_q(x)       // Quantized inference
    Δy   = y_fp - y_q   // Deviation vector
```

**SHALL:** Both models shall process samples in the same order.

**SHALL:** Intermediate activations shall be captured at each layer boundary for per-layer comparison.

**Implementation:**

```c
int cq_verify_dual_inference(const cq_model_fp32_t *model_fp,
                             const cq_model_q16_t *model_q,
                             const float *input,
                             cq_verify_sample_t *result) {
    // FP32 forward pass with intermediate capture
    cq_fp32_forward_with_capture(model_fp, input,
                                  result->fp_intermediates,
                                  &result->fp_output);

    // Quantized forward pass with intermediate capture
    cq_q16_forward_with_capture(model_q, input,
                                 result->q_intermediates,
                                 &result->q_output);

    // Compute deviations
    cq_compute_deviations(result);

    return 0;
}
```

**Verification:** Test  
**Traceability:** CQ-MATH-001 §7.1, CQ-STRUCT-001 §6.1 (ST-006-A)

---

### FR-VER-02: Error Measurement

**Requirement ID:** FR-VER-02  
**Title:** L-Infinity Norm Error Computation  
**Priority:** Critical

**SHALL:** The system shall compute the L-infinity (max absolute) norm of the deviation vector:

```
ε_measured = ‖Δy‖_∞ = max_i |y_fp[i] - y_q[i]|
```

**SHALL:** Compute this metric:
1. Per-layer (comparing intermediate activations)
2. End-to-end (comparing final outputs)
3. Per-sample (tracking worst case across dataset)

**Field Mapping:**

| Metric | Struct Field |
|--------|--------------|
| Layer max error | `cq_layer_comparison_t.error_max_measured` |
| Layer mean error | `cq_layer_comparison_t.error_mean_measured` |
| Total max error | `cq_verification_report_t.total_error_max_measured` |
| Total mean error | `cq_verification_report_t.total_error_mean` |

**Implementation:**

```c
double cq_linf_norm(const float *a, const float *b, size_t n) {
    double max_diff = 0.0;
    for (size_t i = 0; i < n; i++) {
        double diff = fabs((double)a[i] - (double)b[i]);
        if (diff > max_diff) {
            max_diff = diff;
        }
    }
    return max_diff;
}
```

**Verification:** Test  
**Traceability:** CQ-MATH-001 §7.1, CQ-STRUCT-001 §6.1 (ST-006-A)

---

### FR-VER-03: Bound Satisfaction Check

**Requirement ID:** FR-VER-03  
**Title:** Theoretical vs Measured Comparison  
**Priority:** Critical

**SHALL:** The system shall compare measured error against theoretical bound for each layer:

```
bound_satisfied = (error_max_measured ≤ error_bound_theoretical)
```

**SHALL:** Set `cq_layer_comparison_t.bound_satisfied = true` if condition holds.

**SHALL:** Set `cq_verification_report_t.all_bounds_satisfied = true` only if ALL layers satisfy bounds.

**SHALL:** Set `cq_verification_report_t.total_bound_satisfied = true` only if end-to-end error satisfies total bound.

**Verification:** Test  
**Traceability:** CQ-MATH-001 §7.1, CQ-STRUCT-001 §6.1 (ST-006-A)

---

### FR-VER-04: Bound Violation Handling

**Requirement ID:** FR-VER-04  
**Title:** Fail-Closed on Bound Violation  
**Priority:** Critical

**SHALL:** If any bound is violated:

```
error_max_measured > error_bound_theoretical
```

**SHALL:** The system shall:
1. Set `cq_fault_flags_t.bound_violation = 1`
2. Record the violating layer and magnitude of violation
3. Mark verification as **FAILED**
4. Block certificate generation

**Interpretation:** Bound violation means the theoretical analysis was falsified by empirical evidence. This is a fundamental failure requiring investigation.

**Implementation:**

```c
int cq_verify_check_bounds(cq_layer_comparison_t *layer,
                           cq_fault_flags_t *faults) {
    if (layer->error_max_measured > layer->error_bound_theoretical) {
        layer->bound_satisfied = false;
        faults->bound_violation = 1;
        return CQ_FAULT_BOUND_VIOLATION;
    }
    layer->bound_satisfied = true;
    return 0;
}
```

**Verification:** Test  
**Traceability:** CQ-MATH-001 §7.2, CQ-STRUCT-001 §6.2 (ST-006-B)

---

### FR-VER-05: Statistical Aggregation

**Requirement ID:** FR-VER-05  
**Title:** Error Distribution Statistics  
**Priority:** High

**SHALL:** The system shall compute statistics across all verification samples:

| Statistic | Formula | Purpose |
|-----------|---------|---------|
| Max | max(ε_measured) over all samples | Worst-case bound |
| Mean | mean(ε_measured) over all samples | Typical behaviour |
| Std | std(ε_measured) over all samples | Variability |

**SHALL:** Store in `cq_layer_comparison_t`:
- `error_max_measured`
- `error_mean_measured`
- `error_std_measured`

**Verification:** Test  
**Traceability:** CQ-STRUCT-001 §6.1 (ST-006-A)

---

### FR-VER-06: Dataset Hashing

**Requirement ID:** FR-VER-06  
**Title:** Verification Dataset Provenance  
**Priority:** High

**SHALL:** The system shall compute SHA-256 hash of the verification dataset and store in `cq_verification_report_t.verification_set_hash`.

**Note:** Verification dataset SHOULD be different from calibration dataset to avoid overfitting validation.

**Verification:** Test  
**Traceability:** CQ-MATH-001 §9.2, CQ-STRUCT-001 §6.2 (ST-006-B)

---

### FR-VER-07: Quantized Model Loading

**Requirement ID:** FR-VER-07  
**Title:** Binary Model Verification  
**Priority:** High

**SHALL:** The system shall load the quantized model from the binary file produced by Convert (SRS-003-CONVERT).

**SHALL:** Verify model hash matches `cq_model_header_t.quantized_hash` before execution.

**SHALL:** Verify all `cq_layer_header_t.dyadic_valid == true` before execution.

**Verification:** Test  
**Traceability:** CQ-STRUCT-001 §5.4 (ST-005-D)

---

### FR-VER-08: Fixed-Point Inference Engine

**Requirement ID:** FR-VER-08  
**Title:** Deterministic Quantized Execution  
**Priority:** Critical

**SHALL:** The quantized inference engine shall use only integer arithmetic as defined by the DVM (Deterministic Virtual Machine) specification.

**Forbidden Operations:**
- Floating-point arithmetic in inference path
- FMA instructions
- Library math functions
- Data-dependent execution order

**SHALL:** Use `cq_round_rne()` for all fixed-point scaling.

**SHALL:** Use `cq_clamp32()` for all saturation with fault flag propagation.

**Verification:** Test + Code Review  
**Traceability:** CQ-MATH-001 §6

---

## §4 Non-Functional Requirements

### NFR-VER-01: Independence

**SHALL:** Verification shall be independent of the conversion process—it operates only on the binary model file.

**Rationale:** Separation of concerns; verification cannot "cheat" by accessing conversion internals.

### NFR-VER-02: Determinism

**SHALL:** Verification results shall be bit-identical across platforms for the same inputs.

**Rationale:** Verification digest is hashed into the certificate.

### NFR-VER-03: Sample Efficiency

**SHALL:** Verification shall provide meaningful results with ≥100 samples.

**SHOULD:** Support configurable sample count with default 1000.

---

## §5 Interface Specification

### §5.1 Initialisation Interface

```c
/**
 * @brief Initialise verification context.
 * @param ctx          Verification context (caller-allocated).
 * @param model_fp32   FP32 reference model.
 * @param model_q16    Quantized model (loaded from binary).
 * @param analysis     Completed analysis context (for theoretical bounds).
 * @param config       Verification configuration.
 * @return             0 on success, negative error code on failure.
 *
 * @pre  Model hash matches expected
 * @pre  analysis->is_complete == true
 */
int cq_verify_init(cq_verify_ctx_t *ctx,
                   const cq_model_fp32_t *model_fp32,
                   const cq_model_q16_t *model_q16,
                   const cq_analysis_ctx_t *analysis,
                   const cq_verify_config_t *config);
```

### §5.2 Execution Interface

```c
/**
 * @brief Run verification on a single sample.
 * @param ctx      Verification context.
 * @param sample   Input sample data.
 * @param faults   Output: Accumulated fault flags.
 * @return         0 on success, negative error code on failure.
 *
 * @post Per-layer and end-to-end errors updated
 */
int cq_verify_sample(cq_verify_ctx_t *ctx,
                     const float *sample,
                     cq_fault_flags_t *faults);

/**
 * @brief Finalise verification and generate report.
 * @param ctx      Verification context.
 * @param report   Output: Verification report (caller-allocated).
 * @param faults   Output: Accumulated fault flags.
 * @return         0 on success, negative error code on failure.
 *
 * @post report->all_bounds_satisfied indicates pass/fail
 * @post report->total_bound_satisfied indicates end-to-end pass/fail
 */
int cq_verify_finalize(cq_verify_ctx_t *ctx,
                       cq_verification_report_t *report,
                       cq_fault_flags_t *faults);
```

### §5.3 Output Interface

```c
/**
 * @brief Generate verification digest for certificate.
 * @param report   Completed verification report.
 * @param digest   Output: Verification digest (fixed-size).
 * @return         0 on success, negative error code on failure.
 *
 * @pre  report->all_bounds_satisfied == true
 * @pre  report->total_bound_satisfied == true
 */
int cq_verification_digest_generate(const cq_verification_report_t *report,
                                     cq_verification_digest_t *digest);
```

---

## §6 Error Handling

| Condition | Fault Flag | Action |
|-----------|------------|--------|
| Bound violation (any layer) | `bound_violation = 1` | Mark layer failed, continue |
| Bound violation (total) | `bound_violation = 1` | Mark verification failed |
| Model hash mismatch | — | Fail closed |
| Dyadic constraint false | — | Fail closed |
| NaN in output | — | Fail closed |
| Overflow in inference | `overflow = 1` | Record, continue |

---

## §7 Traceability Matrix

| Requirement | CQ-MATH-001 | CQ-STRUCT-001 | Test Case |
|-------------|-------------|---------------|-----------|
| FR-VER-01 | §7.1 | §6.1 | TC-VER-01 |
| FR-VER-02 | §7.1 | §6.1 | TC-VER-02 |
| FR-VER-03 | §7.1 | §6.1 | TC-VER-03 |
| FR-VER-04 | §7.2 | §6.2 | TC-VER-04 |
| FR-VER-05 | — | §6.1 | TC-VER-05 |
| FR-VER-06 | §9.2 | §6.2 | TC-VER-06 |
| FR-VER-07 | — | §5.4 | TC-VER-07 |
| FR-VER-08 | §6 | — | TC-VER-08 |

---

## §8 Acceptance Criteria

| Criterion | Metric | Target |
|-----------|--------|--------|
| L-inf computation | Matches reference implementation | Exact match |
| Bound checking | No false positives (pass when should fail) | 0 false accepts |
| Bound checking | No false negatives (fail when should pass) | < 1% false rejects |
| Dual inference | FP32 and Q16 outputs captured correctly | 100% correct |
| Determinism | Identical results across platforms | Bit-identical |
| Independence | Works from binary model only | No conversion internals |

---

## §9 Verification Feedback Loop

The Verification module closes the mathematical loop established in Analysis:

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Analyze   │────►│   Convert   │────►│   Verify    │
│ (Theorist)  │     │(Transformer)│     │  (Judge)    │
└─────────────┘     └─────────────┘     └──────┬──────┘
       ▲                                       │
       │                                       │
       └───────────────────────────────────────┘
                 Falsification Feedback

If measured > theoretical:
  - Theory is FALSIFIED
  - Root cause analysis required
  - Possible causes:
    1. Analysis bug (bounds too tight)
    2. Conversion bug (incorrect quantization)
    3. Implementation bug (non-determinism)
```

**Principle:** The theoretical bounds from Analysis are claims that Verification can falsify but never prove. A passing verification increases confidence but does not guarantee correctness for all possible inputs.

---

## Document Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Author | William Murray | | |
| Technical Review | | | |
| Quality Assurance | | | |

---

**Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.**
