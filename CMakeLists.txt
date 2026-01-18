cmake_minimum_required(VERSION 3.16)
project(certifiable_quant VERSION 0.1.0 LANGUAGES C)

# ============================================================================
# Certifiable-Quant: Neural Network Quantization Pipeline
# ============================================================================
# Traceability: CQ-MATH-001, CQ-STRUCT-001, SRS-001 through SRS-005
# Compliance: MISRA-C:2012, ISO 26262, IEC 62304, DO-178C
# ============================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags for safety-critical code
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    -fno-fast-math
    -ffp-contract=off
)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# ============================================================================
# Library Sources
# ============================================================================

set(DVM_SOURCES
    src/dvm/primitives.c
)

set(AUDIT_SOURCES
    src/audit/sha256.c
)

set(ANALYZE_SOURCES
    src/analyze/analyze.c
)

set(CONVERT_SOURCES
    src/convert/weight_quant.c
    src/convert/bn_fold.c
)

set(VERIFY_SOURCES
    src/verify/verify.c
)

set(CALIBRATE_SOURCES
    src/calibrate/calibrate.c
)

set(CERTIFICATE_SOURCES
    src/certificate/certificate.c
)

# ============================================================================
# Static Library
# ============================================================================

add_library(certifiable_quant STATIC
    ${DVM_SOURCES}
    ${AUDIT_SOURCES}
    ${ANALYZE_SOURCES}
    ${CALIBRATE_SOURCES}
    ${CONVERT_SOURCES}
    ${VERIFY_SOURCES}
    ${CERTIFICATE_SOURCES}
)

target_include_directories(certifiable_quant PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# ============================================================================
# Unit Tests
# ============================================================================

enable_testing()

# Test: DVM Primitives
add_executable(test_primitives tests/unit/test_primitives.c)
target_link_libraries(test_primitives certifiable_quant m)
add_test(NAME test_primitives COMMAND test_primitives)

# Test: Analyze Module
add_executable(test_analyze tests/unit/test_analyze.c)
target_link_libraries(test_analyze certifiable_quant m)
add_test(NAME test_analyze COMMAND test_analyze)

# Test: Calibrate Module
add_executable(test_calibrate tests/unit/test_calibrate.c)
target_link_libraries(test_calibrate certifiable_quant m)
add_test(NAME test_calibrate COMMAND test_calibrate)

# Test: Convert Module
add_executable(test_convert tests/unit/test_convert.c)
target_link_libraries(test_convert certifiable_quant m)
add_test(NAME test_convert COMMAND test_convert)

# Test: Verify Module
add_executable(test_verify tests/unit/test_verify.c)
target_link_libraries(test_verify certifiable_quant m)
add_test(NAME test_verify COMMAND test_verify)

# Test: Certificate Module
add_executable(test_certificate tests/unit/test_certificate.c)
target_link_libraries(test_certificate certifiable_quant m)
add_test(NAME test_certificate COMMAND test_certificate)

# Test: Bit Identity (cross-platform determinism)
add_executable(test_bit_identity tests/unit/test_bit_identity.c)
target_link_libraries(test_bit_identity certifiable_quant m)
add_test(NAME test_bit_identity COMMAND test_bit_identity)

# ============================================================================
# Custom Target: Run All Tests
# ============================================================================

add_custom_target(test-all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_primitives test_analyze test_calibrate test_convert test_verify test_certificate test_bit_identity
)
