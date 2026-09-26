#ifndef PEACEKEEPER_SIMD
#define PEACEKEEPER_SIMD

/*
 * Peacekeeper SIMD backend.
 *
 * x86: AVX/AVX2/AVX512
 * ARM64: NEON (mandatory in AArch64)
 *
 * The ARM path intentionally implements the same arithmetic as the x86
 * _mm*_madd_epi16 path. No search/evaluation constants are changed.
 */

#if defined(__aarch64__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SIMD
#define PEACEKEEPER_NEON
#endif

#if !defined(PEACEKEEPER_NEON) && (defined(__AVX__) || defined(__AVX2__) || (defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512DQ__)))
#include <immintrin.h>
#define SIMD
#define PEACEKEEPER_X86_SIMD
#endif

#ifdef PEACEKEEPER_NEON

#define BIT_ALIGNMENT 128
#define I16_STRIDE 8
#define ALIGNMENT 16

using register_type = int16x8_t;

inline register_type register_add_16(register_type a, register_type b) {
    return vaddq_s16(a, b);
}
inline register_type register_sub_16(register_type a, register_type b) {
    return vsubq_s16(a, b);
}
inline register_type register_min_16(register_type a, register_type b) {
    return vminq_s16(a, b);
}
inline register_type register_max_16(register_type a, register_type b) {
    return vmaxq_s16(a, b);
}
inline register_type register_set_16(i16 x) {
    return vdupq_n_s16(x);
}
inline register_type register_mul_16(register_type a, register_type b) {
    return vmulq_s16(a, b);
}

/*
 * Keep the register API bit-compatible with the x86 implementation.
 * nnue.cpp stores the 32-bit accumulator in the same SIMD register type.
 */
inline register_type register_add_32(register_type a, register_type b) {
    return vreinterpretq_s16_s32(
        vaddq_s32(vreinterpretq_s32_s16(a), vreinterpretq_s32_s16(b)));
}

inline register_type register_sub_32(register_type a, register_type b) {
    return vreinterpretq_s16_s32(
        vsubq_s32(vreinterpretq_s32_s16(a), vreinterpretq_s32_s16(b)));
}

/*
 * Equivalent to _mm_madd_epi16:
 * [a0*b0+a1*b1, a2*b2+a3*b3, a4*b4+a5*b5, a6*b6+a7*b7]
 *
 * The four i32 results are stored bit-for-bit inside the 128-bit
 * register_type, just like the x86 SIMD backend does.
 */
inline register_type register_madd_16(register_type a, register_type b) {
    const int32x4_t lo = vmull_s16(vget_low_s16(a), vget_low_s16(b));
    const int32x4_t hi = vmull_s16(vget_high_s16(a), vget_high_s16(b));
    return vreinterpretq_s16_s32(vpaddq_s32(lo, hi));
}

inline int32_t register_sum_32(register_type reg) {
    return vaddvq_s32(vreinterpretq_s32_s16(reg));
}

#else

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512DQ__)
#define BIT_ALIGNMENT 512
#elif defined(__AVX2__) || defined(__AVX__)
#define BIT_ALIGNMENT 256
#endif

#ifdef PEACEKEEPER_X86_SIMD

#define I16_STRIDE (BIT_ALIGNMENT / 16)
#define ALIGNMENT (BIT_ALIGNMENT / 8)

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512DQ__)
using register_type = __m512i;
#define register_madd_16 _mm512_madd_epi16
#define register_add_32 _mm512_add_epi32
#define register_sub_32 _mm512_sub_epi32
#define register_add_16 _mm512_add_epi16
#define register_sub_16 _mm512_sub_epi16
#define register_min_16 _mm512_min_epi16
#define register_max_16 _mm512_max_epi16
#define register_set_16 _mm512_set1_epi16
#define register_mul_16 _mm512_mullo_epi16
#elif defined(__AVX2__) || defined(__AVX__)
using register_type = __m256i;
#define register_madd_16 _mm256_madd_epi16
#define register_add_32 _mm256_add_epi32
#define register_sub_32 _mm256_sub_epi32
#define register_add_16 _mm256_add_epi16
#define register_sub_16 _mm256_sub_epi16
#define register_min_16 _mm256_min_epi16
#define register_max_16 _mm256_max_epi16
#define register_set_16 _mm256_set1_epi16
#define register_mul_16 _mm256_mullo_epi16
#endif

#ifdef SIMD
inline int32_t register_sum_32(register_type& reg) {
#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512DQ__)
    const __m256i reduced_8 = _mm256_add_epi32(_mm512_castsi512_si256(reg), _mm512_extracti32x8_epi32(reg, 1));
#elif defined(__AVX2__) || defined(__AVX__)
    const __m256i reduced_8 = reg;
#endif
    const __m128i reduced_4 = _mm_add_epi32(_mm256_castsi256_si128(reduced_8), _mm256_extractf128_si256(reduced_8, 1));
    __m128i vsum = _mm_add_epi32(reduced_4, _mm_srli_si128(reduced_4, 8));
    vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
    int32_t sums = _mm_cvtsi128_si32(vsum);
    return sums;
}
#endif

#endif
#endif
#endif
