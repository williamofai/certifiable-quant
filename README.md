# certifiable-quant

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/williamofai/certifiable-quant)
[![Tests](https://img.shields.io/badge/tests-7%2F7%20passing-brightgreen)](https://github.com/williamofai/certifiable-quant)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)
[![MISRA Compliance](https://img.shields.io/badge/MISRA--C-2012-blue)](docs/misra-compliance.md)

**Deterministic, certifiable model quantization for safety-critical systems.**

Pure C99. Zero dynamic allocation. Certifiable for DO-178C, IEC 62304, and ISO 26262.

---

## The Problem

Standard quantization tools (TensorFlow Lite, ONNX quantizers) are black boxes:
- Error bounds unknown or unbounded
- No formal proof that quantized model preserves behavior
- Calibration statistics non-reproducible
- No audit trail linking original to quantized weights

For safety-critical systems, "approximately equivalent" isn't certifiable.

**Read more:** [Fixed-Point Neural Networks: The Math Behind Q16.16](https://speytech.com/insights/fixed-point-neural-networks/)

## The Solution

`certifiable-quant` provides **provable quantization** with formal error certificates:

### 1. Theoretical Analysis
Compute error bounds *before* quantization: overflow proofs, range propagation, operator norms.

### 2. Empirical Calibration
Collect min/max statistics from representative data with coverage metrics and degenerate detection.

### 3. Verified Conversion
Quantize FP32→Q16.16 with formal error bounds and verify against theoretical limits.

### 4. Cryptographic Certificate
Generate proof object with Merkle root linking analysis, calibration, and verification digests.

**Result:** A certificate proving the quantized model is within ε of the original, auditable forever.

## Status

**All core modules complete — 7/7 test suites passing.**

| Module | Description | Status |
|--------|-------------|--------|
| DVM Primitives | Fixed-point arithmetic with fault detection | ✅ |
| Analyze | Theoretical error bounds, overflow proofs | ✅ |
| Calibrate | Runtime statistics, coverage metrics | ✅ |
| Convert | FP32→Q16.16 with BatchNorm folding | ✅ |
| Verify | Check quantized values against bounds | ✅ |
| Certificate | Merkle-rooted proof generation | ✅ |
| Bit Identity | Cross-platform reproducibility tests | ✅ |

## Quick Start

### Build

```bash
mkdir build && cd build
cmake ..
make
make test  # Run all 7 test suites
```

### Expected Output

```
100% tests passed, 0 tests failed out of 7
Total Test time (real) = 0.02 sec
```

### Basic Quantization Pipeline

```c
#include "cq_types.h"
#include "analyze.h"
#include "calibrate.h"
#include "convert.h"
#include "verify.h"
#include "certificate.h"

ct_fault_flags_t faults = {0};

// 1. Analyze: compute theoretical bounds
cq_analysis_ctx_t analysis;
cq_analysis_init(&analysis, num_layers);
cq_compute_weight_range(weights, n, &w_min, &w_max);
cq_compute_overflow_proof(w_max, x_max, n, &overflow_proof);
cq_analysis_digest_generate(&analysis, &analysis_digest);

// 2. Calibrate: collect runtime statistics
cq_tensor_stats_t stats;
cq_tensor_stats_init(&stats, CQ_FORMAT_Q16);
for (int i = 0; i < num_samples; i++) {
    cq_tensor_stats_update(&stats, activations[i], size);
}
cq_calibration_digest_generate(&calib, &calib_digest);

// 3. Convert: quantize with error tracking
cq_convert_ctx_t convert;
cq_convert_init(&convert, CQ_FORMAT_Q16);
cq_quantize_tensor(fp32_weights, q16_weights, n, &convert, &faults);

// 4. Verify: check against bounds
cq_verify_ctx_t verify;
cq_verify_init(&verify, &analysis);
bool passed = cq_verify_tensor(q16_weights, &analysis.contracts[layer], &verify);
cq_verify_digest_generate(&verify, &verify_digest);

// 5. Certificate: generate proof
cq_certificate_builder_t builder;
cq_certificate_builder_init(&builder);
cq_certificate_builder_set_analysis(&builder, &analysis_digest);
cq_certificate_builder_set_calibration(&builder, &calib_digest);
cq_certificate_builder_set_verification(&builder, &verify_digest);

cq_certificate_t cert;
cq_certificate_build(&builder, &cert, &faults);

// Certificate contains Merkle root proving entire pipeline
```

## Architecture

### Deterministic Virtual Machine (DVM)

All arithmetic operations use widening and saturation:

```c
// CORRECT: Explicit widening
int64_t wide = (int64_t)a * (int64_t)b;
return dvm_round_shift_rne(wide, 16, &faults);

// FORBIDDEN: Raw overflow
return (a * b) >> 16;  // Undefined behavior
```

**Read more:** [From Proofs to Code: Mathematical Transcription in C](https://speytech.com/insights/mathematical-proofs-to-code/)

### Error Analysis Pipeline

| Stage | Computes | Output |
|-------|----------|--------|
| **Analyze** | ε₀ (entry error), εₗ (layer error), Lipschitz bounds | `cq_analysis_digest_t` |
| **Calibrate** | min/max, coverage %, degenerate flags | `cq_calibration_digest_t` |
| **Convert** | Actual quantized values | Quantized tensors |
| **Verify** | Pass/fail against theoretical bounds | `cq_verify_digest_t` |
| **Certificate** | Merkle root of all digests | `cq_certificate_t` |

### Error Bound Theory

Entry error for Q16.16:
```
ε₀ = 2^(-f-1) = 2^(-17) ≈ 7.6 × 10⁻⁶
```

Layer error propagation:
```
ε_{ℓ+1} = ρ_ℓ · ε_ℓ + δ_ℓ
```

Where ρ_ℓ is the Lipschitz constant (operator norm) and δ_ℓ is the layer's quantization error.

**Read more:** [Closure, Totality, and the Algebra of Safe Systems](https://speytech.com/insights/closure-totality-algebra/)

### Fault Model

Every operation signals faults without silent failure:

```c
typedef struct {
    uint32_t overflow    : 1;  /* Saturated high */
    uint32_t underflow   : 1;  /* Saturated low */
    uint32_t div_zero    : 1;  /* Division by zero */
    uint32_t domain      : 1;  /* Invalid input */
    uint32_t precision   : 1;  /* Precision loss detected */
} ct_fault_flags_t;
```

### Certificate Structure

```c
typedef struct {
    uint32_t version;                    /* Format version */
    uint32_t format;                     /* CQ_FORMAT_Q16, etc. */
    cq_hash_t analysis_digest;           /* H(analysis results) */
    cq_hash_t calibration_digest;        /* H(calibration stats) */
    cq_hash_t verification_digest;       /* H(verification results) */
    cq_hash_t merkle_root;               /* Root of all digests */
    int32_t total_error_bound;           /* ε_total in Q16.16 */
    uint8_t all_checks_passed;           /* Global pass/fail */
} cq_certificate_t;
```

## Test Coverage

| Module | Tests | Coverage |
|--------|-------|----------|
| DVM Primitives | 8 | Arithmetic, bit identity |
| Analyze | 30 | Overflow proofs, range propagation, norms |
| Calibrate | 28 | Statistics, coverage, degenerate handling |
| Convert | 12 | Quantization, BatchNorm folding |
| Verify | 22 | Bound checking, contract validation |
| Certificate | 26 | Builder, Merkle, serialization |
| Bit Identity | 8 | Cross-platform verification |

**Total: 134 tests**

## Documentation

- **CQ-MATH-001.md** — Mathematical foundations (error theory)
- **CQ-STRUCT-001.md** — Data structure specifications
- **docs/requirements/** — SRS documents:
  - SRS-001-ANALYZE.md
  - SRS-002-CALIBRATE.md
  - SRS-003-CONVERT.md
  - SRS-004-VERIFY.md
  - SRS-005-CERTIFICATE.md

## Related Projects

| Project | Description |
|---------|-------------|
| [certifiable-data](https://github.com/williamofai/certifiable-data) | Deterministic data pipeline |
| [certifiable-training](https://github.com/williamofai/certifiable-training) | Deterministic training engine |
| [certifiable-quant](https://github.com/williamofai/certifiable-quant) | Deterministic quantization |
| [certifiable-deploy](https://github.com/williamofai/certifiable-deploy) | Deterministic model packaging |
| [certifiable-inference](https://github.com/williamofai/certifiable-inference) | Deterministic inference engine |

Together, these projects provide a complete deterministic ML pipeline for safety-critical systems:
```
certifiable-data → certifiable-training → certifiable-quant → certifiable-deploy → certifiable-inference
```

## Why This Matters

### Medical Devices
IEC 62304 Class C requires traceable, validated transformations. "We quantized it and it still works" is not certifiable.

**Read more:** [IEC 62304 Class C: What Medical Device Software Actually Requires](https://speytech.com/insights/iec-62304-class-c/)

### Autonomous Vehicles
ISO 26262 ASIL-D demands provable error bounds. Unbounded quantization error is a safety hazard.

**Read more:** [ISO 26262 and ASIL-D: The Role of Determinism](https://speytech.com/insights/iso-26262-asil-d-determinism/)

### Aerospace
DO-178C Level A requires complete requirements traceability. Every weight transformation must be auditable.

**Read more:** [DO-178C Level A Certification: How Deterministic Execution Can Streamline Certification Effort](https://speytech.com/insights/do178c-certification/)

This is the first ML quantization tool designed from the ground up for safety-critical certification.

## Deep Dives

Want to understand the engineering principles behind certifiable-quant?

**Formal Methods:**
- [Bit-Perfect Reproducibility: Why It Matters and How to Prove It](https://speytech.com/insights/bit-perfect-reproducibility/)
- [From Proofs to Code: Mathematical Transcription in C](https://speytech.com/insights/mathematical-proofs-to-code/)
- [Cryptographic Execution Tracing and Evidentiary Integrity](https://speytech.com/insights/cryptographic-proof-execution/)

**Safety Engineering:**
- [The Real Cost of Dynamic Memory in Safety-Critical Systems](https://speytech.com/insights/dynamic-memory-safety-critical/)
- [Why Your ML Model Gives Different Results Every Tuesday](https://speytech.com/insights/ml-nondeterminism-problem/)

## Compliance Support

This implementation is designed to support certification under:
- **DO-178C** (Aerospace software)
- **IEC 62304** (Medical device software)
- **ISO 26262** (Automotive functional safety)
- **IEC 61508** (Industrial safety systems)

For compliance packages and certification assistance, contact below.

## Contributing

We welcome contributions from systems engineers working in safety-critical domains. See [CONTRIBUTING.md](CONTRIBUTING.md).

**Important:** All contributors must sign a [Contributor License Agreement](CONTRIBUTOR-LICENSE-AGREEMENT.md).

## License

**Dual Licensed:**
- **Open Source:** GNU General Public License v3.0 (GPLv3)
- **Commercial:** Available for proprietary use in safety-critical systems

For commercial licensing and compliance documentation packages, contact below.

## Patent Protection

This implementation is built on the **Murray Deterministic Computing Platform (MDCP)**,
protected by UK Patent **GB2521625.0**.

MDCP defines a deterministic computing architecture for safety-critical systems,
providing:
- Provable execution bounds
- Resource-deterministic operation
- Certification-ready patterns
- Platform-independent behavior

**Read more:** [MDCP vs. Conventional RTOS](https://speytech.com/insights/mdcp-vs-conventional-rtos/)

For commercial licensing inquiries: william@fstopify.com

## About

Built by **SpeyTech** in the Scottish Highlands.

30 years of UNIX infrastructure experience applied to deterministic computing for safety-critical systems.

Patent: UK GB2521625.0 - Murray Deterministic Computing Platform (MDCP)

**Contact:**
William Murray  
william@fstopify.com  
[speytech.com](https://speytech.com)

**More from SpeyTech:**
- [Technical Articles](https://speytech.com/ai-architecture/)
- [Insights](https://speytech.com/insights/)
- [Open Source Projects](https://speytech.com/open-source/)

---

*Building deterministic AI systems for when lives depend on the answer.*

Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.
