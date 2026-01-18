# SRS-002-CALIBRATE: Calibration Module Requirements

**Project:** Certifiable-Quant  
**Document ID:** SRS-002-CALIBRATE  
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
- Parent: CQ-MATH-001 §5 (Calibration and Coverage)
- Data Structures: CQ-STRUCT-001 §4 (Calibration Structures)
- Siblings: SRS-001-ANALYZE, SRS-003-CONVERT, SRS-004-VERIFY, SRS-005-CERTIFICATE

---

## §1 Purpose

The Calibration Module ("The Observer") collects runtime statistics from representative data to validate theoretical assumptions and determine optimal quantization parameters. It populates `cq_tensor_stats_t` and `cq_calibration_report_t`, providing empirical evidence that the claimed safe ranges are valid.

**Core Responsibility:** Observe actual activation distributions and verify they fall within the assumed bounds.

---

## §2 Scope

### §2.1 In Scope

- Running FP32 inference on calibration dataset
- Collecting min/max statistics per tensor
- Computing coverage metrics
- Enforcing range veto (fail-closed)
- Handling degenerate tensors
- Generating calibration digest for certificate

### §2.2 Out of Scope

- Theoretical bound computation (completed by SRS-001-ANALYZE)
- Weight quantization (delegated to SRS-003-CONVERT)
- Quantized model verification (delegated to SRS-004-VERIFY)

---

## §3 Functional Requirements

### FR-CAL-01: Statistics Collection

**Requirement ID:** FR-CAL-01  
**Title:** Per-Tensor Min/Max Recording  
**Priority:** Critical

**SHALL:** The system shall record absolute min/max values for every tensor during calibration inference, storing results in `cq_tensor_stats_t`.

**Fields to Populate:**

```c
stats.tensor_id    = unique identifier
stats.layer_index  = parent layer
stats.min_observed = min(tensor values across all samples)  // L_obs
stats.max_observed = max(tensor values across all samples)  // U_obs
```

**Algorithm:**

```c
void cq_calibrate_update_stats(cq_tensor_stats_t *stats,
                                const float *tensor,
                                size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (tensor[i] < stats->min_observed) {
            stats->min_observed = tensor[i];
        }
        if (tensor[i] > stats->max_observed) {
            stats->max_observed = tensor[i];
        }
    }
}
```

**Initialisation:** `min_observed = +FLT_MAX`, `max_observed = -FLT_MAX`

**Verification:** Test  
**Traceability:** CQ-MATH-001 §5.1, CQ-STRUCT-001 §4.1 (ST-004-A)

---

### FR-CAL-02: Coverage Computation

**Requirement ID:** FR-CAL-02  
**Title:** Coverage Ratio Calculation  
**Priority:** Critical

**SHALL:** The system shall compute the coverage ratio for each tensor:

```
C_t = (U_obs - L_obs) / (U_safe - L_safe)
    = observed_range / claimed_safe_range
```

**SHALL:** Store result in `cq_tensor_stats_t.coverage_ratio`.

**Interpretation:**

| Coverage | Meaning |
|----------|---------|
| C_t = 1.0 | Perfect coverage (observed fills claimed range) |
| C_t < 1.0 | Under-coverage (calibration may be incomplete) |
| C_t > 1.0 | **INVALID** (triggers range veto, see FR-CAL-03) |

**Global Metrics:**

```c
report.global_coverage_min  = min(C_t) across all tensors
report.global_coverage_p10  = 10th percentile of C_t values
report.global_coverage_mean = mean(C_t) across all tensors
```

**Verification:** Test  
**Traceability:** CQ-MATH-001 §5.1, CQ-STRUCT-001 §4.3 (ST-004-C)

---

### FR-CAL-03: Range Veto Enforcement

**Requirement ID:** FR-CAL-03  
**Title:** Over-Range Fail-Closed Logic  
**Priority:** Critical

**SHALL:** The system shall enforce the range containment invariant:

```
[L_obs, U_obs] ⊆ [L_safe, U_safe]
```

**SHALL:** Set `cq_tensor_stats_t.range_veto = true` if:
- `min_observed < min_safe`, OR
- `max_observed > max_safe`

**SHALL:** Set `cq_calibration_report_t.range_veto_triggered = true` if any tensor triggers veto.

**SHALL:** Set `cq_fault_flags_t.range_exceed = 1` on veto.

**Fail-Closed Rule:** If range veto is triggered:
1. Calibration is **rejected**
2. No calibration digest is generated
3. Certificate generation is blocked

**Rationale:** Observed values exceeding claimed bounds falsify the overflow proofs and scaling assumptions established in Analysis.

**Implementation:**

```c
bool cq_tensor_check_range_veto(cq_tensor_stats_t *stats) {
    if (stats->min_observed < stats->min_safe ||
        stats->max_observed > stats->max_safe) {
        stats->range_veto = true;
        return true;  // Veto triggered
    }
    stats->range_veto = false;
    return false;  // Valid
}
```

**Verification:** Test  
**Traceability:** CQ-MATH-001 §5.2, CQ-STRUCT-001 §4.1 (ST-004-A)

---

### FR-CAL-04: Coverage Threshold Enforcement

**Requirement ID:** FR-CAL-04  
**Title:** Minimum Coverage Requirements  
**Priority:** High

**SHALL:** The system shall verify coverage meets configured thresholds:

```
C_min ≥ config.coverage_min_threshold  (default: 0.90)
C_p10 ≥ config.coverage_p10_threshold  (default: 0.95)
```

**SHALL:** Set `cq_calibration_report_t.coverage_veto_triggered = true` if thresholds not met.

**Note:** Coverage veto is a **warning**, not fail-closed. The certificate may still be issued with reduced confidence.

**Verification:** Test  
**Traceability:** CQ-MATH-001 §5.1, CQ-STRUCT-001 §4.2 (ST-004-B)

---

### FR-CAL-05: Degenerate Tensor Handling

**Requirement ID:** FR-CAL-05  
**Title:** Near-Zero Range Detection  
**Priority:** High

**SHALL:** The system shall detect degenerate tensors where:

```
|max_observed - min_observed| < config.degenerate_epsilon
```

**SHALL:** Set `cq_tensor_stats_t.is_degenerate = true` for such tensors.

**SHALL:** For degenerate tensors:
1. Set scale S = 1.0 (identity scaling)
2. Set coverage_ratio = 1.0 (valid by definition)
3. Log warning for audit trail

**Rationale:** Prevents division by zero in coverage computation and SIGFPE in scale calculation.

**Verification:** Test  
**Traceability:** CQ-MATH-001 §5.3, CQ-STRUCT-001 §4.1 (ST-004-A)

---

### FR-CAL-06: Dataset Hashing

**Requirement ID:** FR-CAL-06  
**Title:** Calibration Dataset Provenance  
**Priority:** High

**SHALL:** The system shall compute SHA-256 hash of the calibration dataset and store in `cq_calibration_report_t.dataset_hash`.

**Hash Input:** Canonical byte representation of all calibration samples in order.

**Rationale:** Enables reproducibility and audit trail. Different calibration data produces different hash, ensuring certificate specificity.

**Verification:** Test  
**Traceability:** CQ-MATH-001 §9.2, CQ-STRUCT-001 §4.3 (ST-004-C)

---

### FR-CAL-07: Sample Count Validation

**Requirement ID:** FR-CAL-07  
**Title:** Minimum Sample Requirement  
**Priority:** Medium

**SHALL:** The system shall verify:

```
sample_count ≥ config.min_samples  (default: 100)
```

**SHALL:** Issue warning if sample count is below threshold.

**Rationale:** Statistical significance of coverage metrics depends on adequate sampling.

**Verification:** Test  
**Traceability:** CQ-STRUCT-001 §4.2 (ST-004-B)

---

## §4 Non-Functional Requirements

### NFR-CAL-01: Determinism

**SHALL:** For identical model, dataset, and configuration, calibration results shall be bit-identical across platforms.

**Rationale:** Calibration digest is hashed into the certificate.

### NFR-CAL-02: Memory Efficiency

**SHALL:** Statistics collection shall use O(T) memory where T is the number of tensors, not O(T × N) where N is sample count.

**Rationale:** Only min/max are stored, not full distributions.

### NFR-CAL-03: Progress Reporting

**SHOULD:** The system should report progress during calibration for large datasets.

---

## §5 Interface Specification

### §5.1 Initialisation Interface

```c
/**
 * @brief Initialise calibration context.
 * @param ctx       Calibration context (caller-allocated).
 * @param model     FP32 model to calibrate.
 * @param config    Calibration configuration.
 * @param analysis  Completed analysis context (for safe ranges).
 * @return          0 on success, negative error code on failure.
 *
 * @pre  analysis->is_complete == true
 * @post ctx->tensors array is initialised with safe ranges from analysis
 */
int cq_calibrate_init(cq_calibrate_ctx_t *ctx,
                      const cq_model_fp32_t *model,
                      const cq_calibrate_config_t *config,
                      const cq_analysis_ctx_t *analysis);
```

### §5.2 Execution Interface

```c
/**
 * @brief Run calibration on a single sample.
 * @param ctx       Calibration context.
 * @param sample    Input sample data.
 * @param faults    Output: Accumulated fault flags.
 * @return          0 on success, negative error code on failure.
 *
 * @post Tensor statistics updated with observations from this sample
 */
int cq_calibrate_sample(cq_calibrate_ctx_t *ctx,
                        const float *sample,
                        cq_fault_flags_t *faults);

/**
 * @brief Finalise calibration and generate report.
 * @param ctx       Calibration context.
 * @param report    Output: Calibration report (caller-allocated).
 * @param faults    Output: Accumulated fault flags.
 * @return          0 on success, negative error code on failure.
 *
 * @pre  At least config.min_samples have been processed
 * @post report->range_veto_triggered indicates pass/fail
 * @post Coverage metrics computed
 */
int cq_calibrate_finalize(cq_calibrate_ctx_t *ctx,
                          cq_calibration_report_t *report,
                          cq_fault_flags_t *faults);
```

### §5.3 Output Interface

```c
/**
 * @brief Generate calibration digest for certificate.
 * @param report    Completed calibration report.
 * @param digest    Output: Calibration digest (fixed-size).
 * @return          0 on success, negative error code on failure.
 *
 * @pre  report->range_veto_triggered == false
 */
int cq_calibration_digest_generate(const cq_calibration_report_t *report,
                                    cq_calibration_digest_t *digest);
```

---

## §6 Error Handling

| Condition | Fault Flag | Action |
|-----------|------------|--------|
| Observed > Safe range | `range_exceed = 1` | Fail closed |
| Coverage below threshold | — | Warning, continue |
| Degenerate tensor | — | Warning, use identity scale |
| Sample count too low | — | Warning, continue |
| NaN/Inf in tensor | — | Skip value, log warning |

---

## §7 Traceability Matrix

| Requirement | CQ-MATH-001 | CQ-STRUCT-001 | Test Case |
|-------------|-------------|---------------|-----------|
| FR-CAL-01 | §5.1 | §4.1 | TC-CAL-01 |
| FR-CAL-02 | §5.1 | §4.3 | TC-CAL-02 |
| FR-CAL-03 | §5.2 | §4.1 | TC-CAL-03 |
| FR-CAL-04 | §5.1 | §4.2 | TC-CAL-04 |
| FR-CAL-05 | §5.3 | §4.1 | TC-CAL-05 |
| FR-CAL-06 | §9.2 | §4.3 | TC-CAL-06 |
| FR-CAL-07 | — | §4.2 | TC-CAL-07 |

---

## §8 Acceptance Criteria

| Criterion | Metric | Target |
|-----------|--------|--------|
| Min/max accuracy | Exact match to reference | 100% match |
| Coverage computation | Matches manual calculation | Exact match |
| Range veto detection | No false negatives | 0 missed violations |
| Degenerate handling | No SIGFPE or NaN | 100% safe |
| Dataset hash | Reproducible across platforms | Bit-identical |
| Determinism | Identical results across runs | 100% identical |

---

## Document Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Author | William Murray | | |
| Technical Review | | | |
| Quality Assurance | | | |

---

**Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.**
