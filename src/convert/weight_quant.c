/**
 * @file weight_quant.c
 * @project Certifiable-Quant
 * @brief Weight Quantization & Constraint Enforcement
 *
 * @traceability SRS-003-CONVERT (FR-CNV-01, FR-CNV-02, FR-CNV-03)
 * @compliance MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
 *
 * @author William Murray
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 */

#include "convert.h"
#include <math.h>

/* ============================================================================
 * FR-CNV-01: Quantization Kernel (RNE)
 * ============================================================================ */

cq_fixed16_t cq_quantize_weight_rne(float w_fp,
                                    double scale,
                                    cq_fault_flags_t *faults)
{
    /* Scale to fixed-point domain using FP64 */
    double scaled = (double)w_fp * scale;

    /* C99 round() uses ties-away-from-zero; correct to RNE */
    double r = round(scaled);
    double diff = r - scaled;

    if (fabs(diff) == 0.5) {
        int64_t i = (int64_t)r;
        if (i % 2 != 0) {
            if (scaled > 0) r -= 1.0;
            else r += 1.0;
        }
    }

    /* Saturate to int32 range */
    if (r > 2147483647.0) {
        if (faults) faults->overflow = 1;
        return 2147483647;
    }
    if (r < -2147483648.0) {
        if (faults) faults->underflow = 1;
        return (cq_fixed16_t)-2147483648LL;
    }

    return (cq_fixed16_t)r;
}

/* ============================================================================
 * FR-CNV-02: Symmetric Enforcement
 * ============================================================================ */

int cq_verify_symmetric(const cq_tensor_spec_t *spec, cq_fault_flags_t *faults)
{
    if (!spec) return CQ_ERROR_NULL_POINTER;

    if (!spec->is_symmetric) {
        if (faults) faults->asymmetric = 1;
        return CQ_FAULT_ASYMMETRIC_PARAMS;
    }
    return 0;
}

/* ============================================================================
 * FR-CNV-03: Dyadic Constraint
 * ============================================================================ */

int cq_verify_constraints(cq_layer_header_t *hdr, cq_fault_flags_t *faults)
{
    if (!hdr) return CQ_ERROR_NULL_POINTER;

    int ret;

    ret = cq_verify_symmetric(&hdr->weight_spec, faults);
    if (ret != 0) return ret;

    ret = cq_verify_symmetric(&hdr->input_spec, faults);
    if (ret != 0) return ret;

    ret = cq_verify_symmetric(&hdr->bias_spec, faults);
    if (ret != 0) return ret;

    /* Check: bias_exp == weight_exp + input_exp */
    int expected = (int)hdr->weight_spec.scale_exp +
                   (int)hdr->input_spec.scale_exp;

    if ((int)hdr->bias_spec.scale_exp != expected) {
        hdr->dyadic_valid = false;
        return CQ_ERROR_DYADIC_VIOLATION;
    }

    hdr->dyadic_valid = true;
    return 0;
}

/* ============================================================================
 * Batch Conversion
 * ============================================================================ */

int cq_convert_weights(const float *w_fp,
                       cq_fixed16_t *w_q,
                       size_t count,
                       const cq_tensor_spec_t *spec,
                       cq_fault_flags_t *faults)
{
    if (!w_fp || !w_q || !spec || !faults) {
        return CQ_ERROR_NULL_POINTER;
    }

    int ret = cq_verify_symmetric(spec, faults);
    if (ret != 0) return CQ_FAULT_ASYMMETRIC_PARAMS;

    double scale = ldexp(1.0, spec->scale_exp);

    for (size_t i = 0; i < count; i++) {
        w_q[i] = cq_quantize_weight_rne(w_fp[i], scale, faults);
    }

    return 0;
}
