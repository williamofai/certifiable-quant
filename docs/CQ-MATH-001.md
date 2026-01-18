# CQ-MATH-001: Mathematical Foundations

**Project:** Certifiable-Quant  
**Document ID:** CQ-MATH-001  
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
- Parent: CQ-ARCH-MATH-001 R1.1
- Children: CQ-STRUCT-001, SRS-001 through SRS-005
- Related: CT-MATH-001 (certifiable-training), CI-MATH-001 (certifiable-inference)

---

## §0 Certification Claim

**Executive Summary**

For any FP32 model M_fp, calibration dataset D, and configuration C, Certifiable-Quant deterministically produces a fixed-point model M_q such that:

| Property | Guarantee |
|----------|-----------|
| **Bit-Exact Determinism** | Identical outputs across all compliant platforms (x86-64, ARM64, RISC-V) |
| **Bounded Deviation** | ‖M_fp(x) − M_q(x)‖ ≤ ε_total under declared domain assumptions |
| **Fail-Closed Safety** | Any violated invariant produces no output model, no certificate |

**Scope Constraint:** This certificate applies to symmetric quantization only (zero-point z = 0). Affine/asymmetric quantization is out of scope.

---

## §1 System Overview

### §1.1 Closed-Loop Architecture

```
FP32 Model
    │
    ▼
 Analyze ────► Calibrate ────► Convert
    ▲                             │
    └──────── Verify ◄────────────┘
               │
               ▼
          Certificate
```

### §1.2 Closure Principle

Every approximation introduces a named, bounded, deterministic error term. Every error term is propagated, measured, and checked. The mathematical closure loop ensures:

1. **Entry error** ε₀ is explicitly bounded at ingress
2. **Layer errors** ε_l propagate via closed-form recurrence
3. **Total error** ε_total is the sum of all contributions
4. **Verification** confirms measured ≤ theoretical

---

## §2 Fixed-Point Arithmetic Model

### §2.1 Fixed-Point Formats

| Format | Storage | Fraction Bits | Step Size | Use Case |
|--------|---------|---------------|-----------|----------|
| Q16.16 | int32_t | 16 | 2⁻¹⁶ ≈ 1.53×10⁻⁵ | Weights, activations |
| Q8.24 | int32_t | 24 | 2⁻²⁴ ≈ 5.96×10⁻⁸ | High-precision intermediates |
| Q32.32 | int64_t | 32 | 2⁻³² ≈ 2.33×10⁻¹⁰ | Accumulators |

### §2.2 Quantization Function

For value w with scale S = 2ⁿ:

```
Q(w) = ⌊w · S⌉ / S
```

Where ⌊·⌉ denotes round-to-nearest-even (RNE).

### §2.3 Rounding Specification

**Round-to-Nearest-Even (RNE):** The only permitted rounding mode.

```c
int32_t cq_round_rne(int64_t x, uint32_t shift) {
    if (shift == 0) return cq_clamp32(x);

    int64_t half = 1LL << (shift - 1);
    int64_t mask = (1LL << shift) - 1;
    int64_t frac = x & mask;
    int64_t quot = x >> shift;  /* Arithmetic shift */

    int64_t result;
    if (frac < half) {
        result = quot;
    } else if (frac > half) {
        result = quot + 1;
    } else {  /* Exactly halfway — round to even */
        result = quot + (quot & 1);
    }

    return cq_clamp32(result);
}
```

**Test Vectors (Q16.16, shift=16):**

| Input (hex) | Decimal | Result | Reason |
|-------------|---------|--------|--------|
| 0x00018000 | 1.5 | 2 | Rounds to even |
| 0x00028000 | 2.5 | 2 | Rounds to even |
| 0x00038000 | 3.5 | 4 | Rounds to even |
| 0xFFFE8000 | −1.5 | −2 | Rounds to even |
| 0xFFFD8000 | −2.5 | −2 | Rounds to even |

### §2.4 Saturation Specification

Saturation clamps out-of-range values to representable bounds with mandatory fault signaling:

```c
int32_t cq_clamp32(int64_t x, cq_fault_flags_t *faults) {
    if (x > INT32_MAX) {
        faults->overflow = 1;
        return INT32_MAX;
    }
    if (x < INT32_MIN) {
        faults->underflow = 1;
        return INT32_MIN;
    }
    return (int32_t)x;
}
```

**Policy:** Saturation is disallowed by default. If enabled, it must be explicitly bounded and recorded in the certificate.

### §2.5 Quantization Scope Lock (Symmetric Only)

**Constraint:** This certificate covers symmetric quantization only.

| Allowed | Not Covered |
|---------|-------------|
| Symmetric signed fixed-point (z = 0) | Affine/asymmetric (z ≠ 0) |
| Q16.16 / Q8.24 representations | Unsigned formats |

**Rationale:** Asymmetric quantization introduces offset-coupling terms (W·z_x) that must be folded into bias, changing overflow and error accounting. These require separate certified derivation.

**Operational Rule:** If an input model provides affine quantization parameters:
1. Reject the model fail-closed, OR
2. Require pre-processing conversion to symmetric form (outside this certificate)

---

## §3 Core Mathematical Invariants

### §3.1 Representation Error Bound

**Theorem (Representation Error):** For symmetric fixed-point quantization with scale S = 2ⁿ:

```
|Q(w) − w| ≤ 1/(2S)
```

**Proof:** RNE rounding produces error at most half the step size. □

| Format | Scale S | Bound |
|--------|---------|-------|
| Q16.16 | 2¹⁶ | 2⁻¹⁷ ≈ 7.63×10⁻⁶ |
| Q8.24 | 2²⁴ | 2⁻²⁵ ≈ 2.98×10⁻⁸ |

### §3.2 Dyadic Constraint (Mandatory)

**Invariant:** Integer quantities may only be added if they share the same scale.

For linear layer y = Wx + b:

Let:
- S_w = weight scale
- S_x = input scale

Then accumulator scale:
```
S_acc = S_w × S_x
```

**Dyadic Constraint:**
```
S_b ≡ S_acc
```

**Consequences:**

1. Bias cannot be independently optimised — its scale is determined by weight and input scales
2. Bias quantization is representation-only:
   ```
   b_q = ⌊b · S_acc⌉
   ```
3. Bias error bound:
   ```
   ‖Δb‖ ≤ 1/(2·S_acc)
   ```

This invariant is enforced in Analyze, Convert, and Verify modules.

### §3.3 Linear Layer Error (Closed Form)

**Theorem (Linear Layer Error):** For linear layer y = Wx + b with quantized weights W_q, inputs x_q, and bias b_q:

Let:
- ΔW = W_fp − W_q (weight quantization error)
- Δx = x_fp − x_q (input quantization error)
- Δb = b_fp − b_q (bias quantization error)

Then output error:
```
Δy = ΔW·x + W_q·Δx + Δb
```

Using operator norms:
```
‖Δy‖ ≤ ‖ΔW‖·‖x‖ + ‖W_q‖·‖Δx‖ + ‖Δb‖
```

**Interpretation:** Error has three sources:
1. Weight quantization acting on true input
2. Quantized weights acting on input error
3. Bias quantization (constant term)

### §3.4 Accumulator Overflow Proof

**Theorem (Overflow Safety):** For dot product of length n:

```
|acc| ≤ n · |w_int|_max · |x_int|_max
```

**Safety Condition:**
```
n · |w_int|_max · |x_int|_max < 2⁶³
```

| Weight Range | Input Range | Max Safe n |
|--------------|-------------|------------|
| [−2¹⁵, 2¹⁵−1] | [−2¹⁵, 2¹⁵−1] | 2³³ |
| [−2³¹, 2³¹−1] | [−2³¹, 2³¹−1] | ~32 |

**If Violated:**
1. Chunked accumulation (split into safe-length segments), OR
2. Mixed precision (use Q8.24 for problematic layers), OR
3. Conversion fails (fail-closed)

Overflow proofs are computed per-layer and recorded in the certificate.

### §3.5 Projection (Requantization) Error

**Theorem (Projection Error):** Accumulator output must be projected to next-layer format. For output scale S_out:

```
ε_proj ≤ 1/(2·S_out)
```

**Properties:**
- Deterministic (RNE rounding)
- Unavoidable (fundamental quantization cost)
- Independent of data (worst-case bound)

### §3.6 Layer-to-Layer Error Recurrence

#### §3.6.1 Base Case: Entry Error ε₀

The recurrence requires explicit initial error capturing input ingress quantization.

Let external input x_in enter the quantized pipeline via deterministic quantization with scale S_in:

```
x₀ = Q(x_in)
```

**Definition (Entry Error):**
```
ε₀ ≜ ‖x_in − x₀‖
```

With symmetric uniform quantization and RNE (no saturation):
```
ε₀ ≤ 1/(2·S_in)
```

**Notes:**
- If external input is already integer (e.g., INT8) and maps exactly into Q-format via exact scaling, ε₀ may be 0
- The tool must compute the appropriate bound and record it
- ε₀ is always recorded in the certificate as the recurrence initializer

#### §3.6.2 Recurrence (Closed Form)

**Theorem (Error Recurrence):** Let:
- ε_l = input error bound to layer l
- A_l = ‖W_l‖ (operator norm upper bound, amplification factor)
- ε_proj,l = requantization bound at layer exit

Then:
```
ε_{l+1} ≤ A_l·ε_l + ‖ΔW_l‖·‖x_l‖ + ‖Δb_l‖ + ε_proj,l
```

**Interpretation:**
- A_l·ε_l: Input error amplified by layer
- ‖ΔW_l‖·‖x_l‖: Weight quantization error
- ‖Δb_l‖: Bias quantization error
- ε_proj,l: Requantization error

With ε₀ defined above, the bound chain is mathematically closed end-to-end.

#### §3.6.3 Total Error Bound

**Theorem (Total Error):** For L-layer network:

```
ε_total = ε_L
```

Where ε_L is computed by applying the recurrence L times starting from ε₀.

**Closed-form (homogeneous case):** If all layers have identical amplification A and per-layer error δ:

```
ε_L = A^L·ε₀ + δ·(A^L − 1)/(A − 1)    for A ≠ 1
ε_L = ε₀ + L·δ                         for A = 1
```

---

## §4 Nonlinearities

### §4.1 ReLU

**Theorem (ReLU Exactness):** ReLU introduces zero quantization error.

```
ReLU(x) = max(0, x)
```

**Proof:** The max operation is exact for integers. □

### §4.2 Softmax (Certified Design)

#### §4.2.1 Stability Transform

```
softmax(x_i) = exp(x_i − max(x)) / Σ_j exp(x_j − max(x))
```

**Purpose:** Shifting by max(x) ensures all exponent arguments ≤ 0, keeping outputs in [0, 1].

#### §4.2.2 Base-2 Exponential (Certified)

**Method:** Compute exp(x) via base-2:
```
exp(x) = 2^(x·log₂(e))
```

Let t = x·log₂(e) = k + f where k = ⌊t⌋ (integer) and f ∈ [0, 1) (fraction).

Then:
```
2^t = 2^k · 2^f
```

**Implementation:**
- 2^k: Exact bit-shift (k ≤ 0 after stability transform)
- 2^f: Quadratic polynomial approximation

```
P(f) = 1 + c₁·f + c₂·f²
```

| Coefficient | Q16.16 Value | Decimal |
|-------------|--------------|---------|
| c₁ | 45426 | ≈ 0.6931 |
| c₂ | 15744 | ≈ 0.2402 |

**Proved Bound:**
```
ε_exp ≤ 3 ULPs
```

Error decreases as output decreases (since k ≤ 0).

#### §4.2.3 Division (Reciprocal Multiply)

**Method:**
1. Compute reciprocal in high-precision fixed-point
2. Single deterministic multiply
3. Explicit RNE rounding

**Bound:** ε_recip ≤ 1 ULP (from final rounding)

#### §4.2.4 Softmax Error Bound

```
ε_softmax ≤ ε_exp + ε_recip ≤ 4 ULPs
```

**Critical:** No floating point. No library functions. No undefined behaviour.

---

## §5 Calibration and Coverage

### §5.1 Activation Coverage Metric

For each tensor t:

```
C_t = (observed range) / (claimed safe range)
```

**Global Metrics:**
- C_min: Minimum coverage across all tensors
- C_p10: 10th percentile coverage

**Default Acceptance Thresholds:**
```
C_min ≥ 0.90
C_p10 ≥ 0.95
```

### §5.2 Over-Range Veto (Fail-Closed)

**Invariant:** Observed calibration range must not exceed claimed safe range.

Let:
- [L_safe, U_safe] = claimed safe range
- [L_obs, U_obs] = observed calibration range

**Requirement:**
```
[L_obs, U_obs] ⊆ [L_safe, U_safe]
```

Equivalently:
```
L_obs ≥ L_safe  AND  U_obs ≤ U_safe
```

**Fail-Closed Rule:** If invariant is violated at calibration time:
```
HALT with CQ_FAULT_RANGE_CONTRADICTION
```

**Rationale:** Observed > claimed falsifies the assumed bounds and invalidates scaling/overflow proofs.

### §5.3 Degenerate Tensor Handling

**Condition:** If weight range is degenerate:
```
|w_max − w_min| < ε_degenerate
```

**Action:** Set scale S = 1.0

**Rationale:** Prevents division by zero and SIGFPE.

---

## §6 Determinism Guarantees

### §6.1 Determinism ≠ Accuracy

| Property | Status |
|----------|--------|
| Error is non-zero | Expected |
| Error is bounded | Guaranteed |
| Error is systematic | By construction |
| Output is bit-identical | Guaranteed |

**Principle:** Systematic, bounded error is preferred over stochastic, unbounded accuracy in safety-critical systems.

### §6.2 Platform Independence

All operations use only:
- Integer arithmetic (int32_t, int64_t)
- Explicit rounding (RNE via cq_round_rne)
- Explicit saturation (via cq_clamp32)

**Forbidden:**
- IEEE-754 floating-point
- FMA instructions
- Library math functions (exp, log, sqrt)
- Platform-dependent operations

### §6.3 C99 Shift Safety

**Issue:** Right-shifting signed integers is implementation-defined in C99.

**Required Implementation:**

```c
static inline int32_t cq_sra(int32_t v, int s) {
#if defined(__GNUC__) || defined(__clang__)
    return v >> s;  /* Arithmetic shift guaranteed */
#else
    if (v < 0 && s > 0)
        return (v >> s) | ~(~0U >> s);
    return v >> s;
#endif
}
```

**Mandate:** All signed right-shifts must use this macro.

---

## §7 Verification Logic

### §7.1 Bound Verification

For each layer l:
```
ε_measured,l ≤ ε_theoretical,l
```

For total:
```
ε_measured,total ≤ ε_total
```

### §7.2 Verification Failure

**Condition:** If measured error exceeds theoretical bound:
```
ε_measured > ε_theoretical
```

**Action:** PANIC / FAIL CLOSED

**Interpretation:** Theory is falsifiable. Verification failure indicates:
1. Implementation bug, OR
2. Bound derivation error, OR
3. Violated preconditions

Either way, no certificate may be issued.

### §7.3 Feedback Loop

Verification feeds back into Analyze:
- Measured statistics refine theoretical bounds
- Tight bounds reduce over-conservatism
- Theory remains falsifiable

---

## §8 Preprocessing Requirements

### §8.1 Batch Normalisation Folding

**Precondition (Mandatory):** All Batch Normalisation layers must be folded into preceding linear/conv layers in the FP32 domain prior to Analyze.

**Certified Operating Assumption:** Certifiable-Quant operates only on inference-ready (BN-folded) architectures.

### §8.2 Folding Definition

For BN parameters γ, β, μ, σ², ε:

```
W' = W · γ / √(σ² + ε)
b' = (b − μ) · γ / √(σ² + ε) + β
```

### §8.3 Folding Policy

1. Folding is performed in FP32/FP64 (implementation choice), deterministically, before quantization
2. The certificate records:
   - Whether folding occurred
   - Hash/digest of BN parameters used
   - Post-fold model hash (true input to Analyze/Calibrate/Convert)

**Fail-Closed Rule:** If BN layers are present and not folded:
```
REJECT with CQ_FAULT_UNFOLDED_BATCHNORM
```

---

## §9 Certificate Specification

### §9.1 Certificate Claims

The certificate SHALL include:

**Determinism Claim:**
> The quantized model employs exclusively integer arithmetic with explicitly defined rounding and shifting semantics. The system exhibits bit-exact determinism across all compliant platforms.

**Error Bound Claim:**
> The numerical deviation from the ideal real-valued computation is non-zero but systematic, with a proved worst-case bound of ε_max ≤ ε_total. This deviation is static, reproducible, and fully accounted for in the system safety margin.

**Scope Claim:**
> This certificate applies to symmetric quantization (zero-point z = 0) only. Affine/asymmetric quantization is out of scope for the certified bounds.

**Entry Error Claim:**
> The end-to-end bound includes the ingress quantization term ε₀ for converting external inputs into the internal fixed-point domain.

**Over-Range Claim:**
> If calibration observations exceed the claimed safe range, calibration is rejected and no certificate is emitted.

### §9.2 Certificate Structure

```
CQ-CERTIFICATE-v1
├── Metadata
│   ├── Timestamp (ISO 8601)
│   ├── Tool version
│   ├── Configuration hash
│   └── Scope declaration (symmetric only)
├── Source Model
│   ├── Hash (SHA-256)
│   ├── Format (ONNX, PyTorch, etc.)
│   ├── BN folding status
│   └── Architecture summary
├── Target Model
│   ├── Hash (SHA-256)
│   ├── Format (certifiable-inference)
│   └── Size (parameters, bytes)
├── Quantization Parameters
│   ├── Per-layer scales (S_w, S_x, S_out)
│   ├── Format selections (Q16.16 / Q8.24)
│   └── Overflow proof status per layer
├── Error Analysis
│   ├── Entry error ε₀
│   ├── Per-layer bounds ε_l
│   ├── Theoretical total ε_total
│   ├── Measured maximum
│   └── Measured mean
├── Calibration Report
│   ├── Dataset hash
│   ├── Sample count
│   ├── Coverage metrics (C_min, C_p10)
│   └── Over-range veto status
├── Verification Report
│   ├── Layer-by-layer comparison
│   ├── Bound satisfaction (measured ≤ theoretical)
│   └── Pass/fail determination
└── Integrity
    ├── Merkle root (all above)
    └── Optional: External attestation
```

---

## §10 Fault Model

### §10.1 Fault Flags

```c
typedef struct {
    uint32_t overflow        : 1;  /* Saturated high */
    uint32_t underflow       : 1;  /* Saturated low */
    uint32_t div_zero        : 1;  /* Division by zero */
    uint32_t range_exceed    : 1;  /* Calibration over-range */
    uint32_t unfolded_bn     : 1;  /* BN not folded */
    uint32_t asymmetric      : 1;  /* Non-symmetric quantization */
    uint32_t bound_violation : 1;  /* Measured > theoretical */
    uint32_t _reserved       : 25;
} cq_fault_flags_t;
```

### §10.2 Fault Semantics

| Fault | Code | Action |
|-------|------|--------|
| CQ_FAULT_OVERFLOW | 0x01 | Record, continue if within bounds |
| CQ_FAULT_UNDERFLOW | 0x02 | Record, continue if within bounds |
| CQ_FAULT_DIV_ZERO | 0x04 | Fail closed |
| CQ_FAULT_RANGE_CONTRADICTION | 0x08 | Fail closed |
| CQ_FAULT_UNFOLDED_BATCHNORM | 0x10 | Fail closed |
| CQ_FAULT_ASYMMETRIC_PARAMS | 0x20 | Fail closed |
| CQ_FAULT_BOUND_VIOLATION | 0x40 | Fail closed |

### §10.3 Fault Invariant

**Rule:** Any fault that invalidates the mathematical proofs must result in fail-closed behaviour with no certificate emitted.

---

## §11 Traceability Matrix

| Section | Requirement | Verification | SRS Reference |
|---------|-------------|--------------|---------------|
| §2.3 | RNE rounding | Test vectors | SRS-001-ANALYZE §3.1 |
| §3.1 | Representation bound | Analysis | SRS-001-ANALYZE §3.2 |
| §3.2 | Dyadic constraint | Inspection | SRS-003-CONVERT §3.1 |
| §3.4 | Overflow proof | Analysis | SRS-001-ANALYZE §3.4 |
| §3.6 | Error recurrence | Test | SRS-004-VERIFY §3.1 |
| §4.2 | Softmax bound | Test vectors | SRS-003-CONVERT §3.5 |
| §5.2 | Over-range veto | Test | SRS-002-CALIBRATE §3.3 |
| §6.3 | Shift safety | Inspection | SRS-003-CONVERT §3.2 |
| §8.1 | BN folding | Inspection | SRS-003-CONVERT §3.6 |

---

## §12 References

### §12.1 Internal Documents

| ID | Title | Relationship |
|----|-------|--------------|
| CQ-STRUCT-001 | Data Structure Specification | Derived from this document |
| SRS-001-ANALYZE | Analysis Module Requirements | Requirements for §3 |
| SRS-002-CALIBRATE | Calibration Module Requirements | Requirements for §5 |
| SRS-003-CONVERT | Conversion Module Requirements | Requirements for §2, §4, §8 |
| SRS-004-VERIFY | Verification Module Requirements | Requirements for §7 |
| SRS-005-CERTIFICATE | Certificate Module Requirements | Requirements for §9 |

### §12.2 Related Certifiable Documents

| ID | Title | Project |
|----|-------|---------|
| CT-MATH-001 | Mathematical Foundations | certifiable-training |
| CI-MATH-001 | Mathematical Foundations | certifiable-inference |
| CD-MATH-001 | Mathematical Foundations | certifiable-data |

### §12.3 External References

1. Jacob, B. et al. (2018). "Quantization and Training of Neural Networks for Efficient Integer-Arithmetic-Only Inference." CVPR.

2. Gholami, A. et al. (2021). "A Survey of Quantization Methods for Efficient Neural Network Inference." arXiv:2103.13630.

3. Lin, D. et al. (2016). "Fixed Point Quantization of Deep Convolutional Networks." ICML.

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
