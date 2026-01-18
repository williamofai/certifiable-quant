# Contributing to Certifiable Quant

Thank you for your interest! We are building the world's first certifiable model quantization tool for safety-critical ML applications.

## 1. The Legal Bit (CLA)

All contributors must sign our **Contributor License Agreement (CLA)**.

**Why?** It allows SpeyTech to provide commercial licenses to companies that cannot use GPL code while keeping the project open source.

**How?** Our [CLA Assistant](https://cla-assistant.io/) will prompt you when you open your first Pull Request.

## 2. Coding Standards

All code must adhere to our **DVM Compliance Guidelines**:

- **No Dynamic Allocation:** Do not use `malloc`, `free`, or `realloc`
- **MISRA-C Compliance:** Follow MISRA-C:2012 guidelines
- **Explicit Types:** Use `int32_t`, `uint32_t`, not `int` or `long`
- **No Floating-Point:** Quantization path must be pure fixed-point arithmetic
- **Bounded Loops:** All loops must have provable upper bounds
- **DVM Primitives Only:** Use `dvm_add32()`, `dvm_mul_q16()`, etc.

## 3. The Definition of Done

A PR is only merged when:

1. ✅ It is linked to a **Requirement ID** in the SRS documents
2. ✅ It has **100% Branch Coverage** in unit tests
3. ✅ It passes our **Bit-Perfect Test** (identical output on x86, ARM, RISC-V)
4. ✅ It is **MISRA-C compliant**
5. ✅ It traces to **CQ-MATH-001** or **CQ-STRUCT-001**
6. ✅ It has been reviewed by the Project Lead

## 4. Documentation

Every function must document:
- Purpose
- Preconditions
- Postconditions
- Complexity (O(1), O(n), etc.)
- Determinism guarantee
- Traceability reference

Example:
```c
/**
 * @brief Compute overflow proof for accumulator chain
 *
 * @traceability CQ-MATH-001 §4, SRS-001-ANALYZE
 *
 * Precondition: w_max, x_max, n all non-negative
 * Postcondition: proof.is_safe true iff n × w_max × x_max < 2^63
 * Complexity: O(1) time, O(1) space
 * Determinism: Bit-perfect across all platforms
 */
bool cq_compute_overflow_proof(uint32_t w_max, uint32_t x_max, uint32_t n, cq_overflow_proof_t *proof);
```

## 5. DVM Compliance

All quantization-path code must use DVM primitives (see `include/dvm.h`):

- `dvm_add32()`, `dvm_sub32()` — Saturating arithmetic
- `dvm_mul_q16()`, `dvm_div_q16()` — Q16.16 fixed-point
- `dvm_round_shift_rne()` — Round-to-nearest-even
- `dvm_clamp32()` — Explicit saturation

**Never** use raw `+`, `-`, `*` on `int32_t` without explicit overflow handling.

## 6. Fault Handling

All arithmetic operations must:
1. Accept a `ct_fault_flags_t *faults` parameter
2. Set appropriate flags on overflow/underflow
3. Return a deterministic value (even on fault)

## 7. Test Requirements

Every module needs:
- **Unit tests**: Production-grade test suite (see `tests/unit/`)
- **Test vectors**: Exact values from CQ-MATH-001 specification
- **Cross-platform**: Verify bit-identity on x86, ARM, RISC-V
- **Coverage**: RUN_TEST() macro with clear pass/fail output

## 8. Getting Started

Look for issues labeled `good-first-issue` or `dvm-layer`.

We recommend starting with:
- DVM primitive tests (test vectors from CQ-MATH-001)
- Analyze module error bound tests
- Verify module contract validation tests

## Questions?

- **Technical questions:** Open an issue
- **General inquiries:** william@fstopify.com
- **Security issues:** Email william@fstopify.com (do not open public issues)

Thank you for helping make certifiable quantization a reality! 🎯
