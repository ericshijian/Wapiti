/*
 *      Wapiti - A linear-chain CRF tool
 *
 * Copyright (c) 2009-2010  CNRS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the CNRS, nor the names of its contributors may be
 *       used to endorse or promote products derived from this software without
 *       specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#include "wapiti.h"
#include "tools.h"
#include "vmath.h"

#if defined(WAP_USE_SSE)
  #if defined(__SSE3__)
    #define XVM_SSE3
    #define XVM_SSE2
    #define XVM_SSE1
  #elif defined(__SSE2__)
    #define XVM_SSE2
    #define XVM_SSE1
  #elif defined(__SSE__)
    #define XVM_SSE1
    #define XVM_ANSI
  #endif
#else
  #define XVM_ANSI
#endif

#if defined(WAP_PREC_SGL)
  #define XVM_32
#elif defined(WAP_PREC_DBL)
  #define XVM_64
#else
  #error "no floating point precision defined"
#endif

#ifdef XVM_SSE3
#include <pmmintrin.h>
#endif
#ifdef XVM_SSE2
#include <emmintrin.h>
#endif
#ifdef XVM_SSE1
#include <xmmintrin.h>
#endif

/* xvm_mode:
 *   Return a string describing the SSE level used in the optimized code paths.
 */
const char *xvm_mode(void) {
#if defined(XVM_SSE3)
	return "sse3";
#elif defined(XVM_SSE2)
	return "sse2";
#elif defined(XVM_SSE1)
	return "sse1";
#else
	return "no-sse";
#endif
}

/* xvm_new:
 *   Allocate a new vector suitable to be used in the SSE code paths. This
 *   ensure that the vector size contains the need padding. You must only use
 *   vector allocated by this function if you use the optimized code paths.
 */
real *xvm_new(size_t N) {
#ifdef XVM_ANSI
	return xmalloc(sizeof(real) * N);
#else
  #ifdef XVM_32
	if (N % 8 != 0)
		N += 8 - N % 8;
  #else
	if (N % 4 != 0)
		N += 4 - N % 4;
  #endif
	void *ptr = _mm_malloc(sizeof(real) * N, 16);
	if (ptr == NULL)
		fatal("out of memory");
	return ptr;
#endif
}

/* xvm_free:
 *   Free a vector allocated by xvm_new.
 */
void xvm_free(real x[]) {
#ifdef XVM_ANSI
	free(x);
#else
	_mm_free(x);
#endif
}

/* xvm_neg:
 *   Return the component-wise negation of the given vector:
 *       r = -x
 */
void xvm_neg(real r[], const real x[], size_t N) {
#if defined(XVM_32) && defined(XVM_SSE1)
	assert(r != NULL && ((size_t)r % 16) == 0);
	assert(x != NULL && ((size_t)x % 16) == 0);
	const __m128 vz = _mm_setzero_ps();
	for (size_t n = 0; n < N; n += 8) {
		const __m128 x0 = _mm_load_ps(x + n    );
		const __m128 x1 = _mm_load_ps(x + n + 4);
		const __m128 r0 = _mm_sub_ps(vz, x0);
		const __m128 r1 = _mm_sub_ps(vz, x1);
		_mm_store_ps(r + n,     r0);
		_mm_store_ps(r + n + 4, r1);
	}
#elif defined(XVM_64) && defined(XVM_SSE2)
	assert(r != NULL && ((size_t)r % 16) == 0);
	assert(x != NULL && ((size_t)x % 16) == 0);
	const __m128d vz = _mm_setzero_pd();
	for (size_t n = 0; n < N; n += 4) {
		const __m128d x0 = _mm_load_pd(x + n    );
		const __m128d x1 = _mm_load_pd(x + n + 2);
		const __m128d r0 = _mm_sub_pd(vz, x0);
		const __m128d r1 = _mm_sub_pd(vz, x1);
		_mm_store_pd(r + n,     r0);
		_mm_store_pd(r + n + 2, r1);
	}
#else
	for (size_t n = 0; n < N; n++)
		r[n] = -x[n];
#endif
}

/* xvm_sub:
 *   Return the difference of the two given vector:
 *       r = x .- y
 */
void xvm_sub(real r[], const real x[], const real y[], size_t N) {
#if defined(XVM_32) && defined(XVM_SSE1)
	assert(r != NULL && ((size_t)r % 16) == 0);
	assert(x != NULL && ((size_t)x % 16) == 0);
	assert(y != NULL && ((size_t)y % 16) == 0);
	for (size_t n = 0; n < N; n += 8) {
		const __m128 x0 = _mm_load_ps(x + n    );
		const __m128 x1 = _mm_load_ps(x + n + 4);
		const __m128 y0 = _mm_load_ps(y + n    );
		const __m128 y1 = _mm_load_ps(y + n + 4);
		const __m128 r0 = _mm_sub_ps(x0, y0);
		const __m128 r1 = _mm_sub_ps(x1, y1);
		_mm_store_ps(r + n,     r0);
		_mm_store_ps(r + n + 4, r1);
	}
#elif defined(XVM_64) && defined(XVM_SSE2)
	assert(r != NULL && ((size_t)r % 16) == 0);
	assert(x != NULL && ((size_t)x % 16) == 0);
	assert(y != NULL && ((size_t)y % 16) == 0);
	for (size_t n = 0; n < N; n += 4) {
		const __m128d x0 = _mm_load_pd(x + n    );
		const __m128d x1 = _mm_load_pd(x + n + 2);
		const __m128d y0 = _mm_load_pd(y + n    );
		const __m128d y1 = _mm_load_pd(y + n + 2);
		const __m128d r0 = _mm_sub_pd(x0, y0);
		const __m128d r1 = _mm_sub_pd(x1, y1);
		_mm_store_pd(r + n,     r0);
		_mm_store_pd(r + n + 2, r1);
	}
#else
	for (size_t n = 0; n < N; n++)
		r[n] = x[n] - y[n];
#endif
}

/* xvm_scale:
 *   Return the given vector scaled by a constant:
 *     r = a * x
 */
void xvm_scale(real r[], const real x[], real a, size_t N) {
	for (size_t n = 0; n < N; n++)
		r[n] = x[n] * a;
}

/* xvm_norm:
 *   Store a normalized copy of the given vector in r and return the
 *   normalization factor.
 */
real xvm_unit(real r[], const real x[], size_t N) {
	real sum = 0.0;
	for (size_t n = 0; n < N; n++)
		sum += x[n];
	const real scale = 1.0 / sum;
	xvm_scale(r, x, scale, N);
	return scale;
}

/* xvm_norm:
 *   Return the euclidian norm of the given vector.
 */
real xvm_norm(const real x[], size_t N) {
	real r = 0.0;
#if defined(XVM_32) && defined(XVM_SSE1)
	assert(x != NULL && ((size_t)x % 16) == 0);
	size_t n, d = N % 8;
	__m128 s0 = _mm_setzero_ps();
	__m128 s1 = _mm_setzero_ps();
	for (n = 0; n < N - d; n += 8) {
		const __m128 x0 = _mm_load_ps(x + n    );
		const __m128 x1 = _mm_load_ps(x + n + 4);
		const __m128 r0 = _mm_mul_ps(x0, x0);
		const __m128 r1 = _mm_mul_ps(x1, x1);
		s0 = _mm_add_ps(s0, r0);
		s1 = _mm_add_ps(s1, r1);
	}
	s0 = _mm_add_ps(s0, s1);
  #if defined(XVM_SSE3)
	s0 = _mm_hadd_ps(s0, s0);
	s0 = _mm_hadd_ps(s0, s0);
  #else
	s1 = s0;
	s0 = _mm_shuffle_ps(s0, s1, _MM_SHUFFLE(1, 0, 3, 2));
	s0 = _mm_add_ps(s0, s1);
	s1 = s0;
	s0 = _mm_shuffle_ps(s0, s1, _MM_SHUFFLE(2, 3, 0, 1));
	s0 = _mm_add_ps(s0, s1);
  #endif
	_mm_store_ss(&r, s0);
	for ( ; n < N; n++)
		r += x[n] * x[n];
#elif defined(XVM_64) && defined(XVM_SSE2)
	assert(x != NULL && ((size_t)x % 16) == 0);
	size_t n, d = N % 4;
	__m128d s0 = _mm_setzero_pd();
	__m128d s1 = _mm_setzero_pd();
	for (n = 0; n < N - d; n += 4) {
		const __m128d x0 = _mm_load_pd(x + n    );
		const __m128d x1 = _mm_load_pd(x + n + 2);
		const __m128d r0 = _mm_mul_pd(x0, x0);
		const __m128d r1 = _mm_mul_pd(x1, x1);
		s0 = _mm_add_pd(s0, r0);
		s1 = _mm_add_pd(s1, r1);
	}
	s0 = _mm_add_pd(s0, s1);
	s1 = _mm_shuffle_pd(s0, s0, _MM_SHUFFLE2(1, 1));
	s0 = _mm_add_pd(s0, s1);
	_mm_store_sd(&r, s0);
	for ( ; n < N; n++)
		r += x[n] * x[n];
#else
	for (size_t n = 0; n < N; n++)
		r += x[n] * x[n];
#endif
#if defined(XVM_32)
	return sqrtf(r);
#else
	return sqrt(r);
#endif
}

/* xvm_dot:
 *   Return the dot product of the two given vectors.
 */
real xvm_dot(const real x[], const real y[], size_t N) {
	real r = 0.0;
#if defined(XVM_32) && defined(XVM_SSE1)
	assert(x != NULL && ((size_t)x % 16) == 0);
	assert(y != NULL && ((size_t)y % 16) == 0);
	size_t n, d = N % 8;
	__m128 s0 = _mm_setzero_ps();
	__m128 s1 = _mm_setzero_ps();
	for (n = 0; n < N - d; n += 8) {
		const __m128 x0 = _mm_load_ps(x + n    );
		const __m128 x1 = _mm_load_ps(x + n + 4);
		const __m128 y0 = _mm_load_ps(y + n    );
		const __m128 y1 = _mm_load_ps(y + n + 4);
		const __m128 r0 = _mm_mul_ps(x0, y0);
		const __m128 r1 = _mm_mul_ps(x1, y1);
		s0 = _mm_add_ps(s0, r0);
		s1 = _mm_add_ps(s1, r1);
	}
	s0 = _mm_add_ps(s0, s1);
  #if __SSE__ >= 3
	s0 = _mm_hadd_ps(s0, s0);
	s0 = _mm_hadd_ps(s0, s0);
  #else
	s1 = s0;
	s0 = _mm_shuffle_ps(s0, s1, _MM_SHUFFLE(1, 0, 3, 2));
	s0 = _mm_add_ps(s0, s1);
	s1 = s0;
	s0 = _mm_shuffle_ps(s0, s1, _MM_SHUFFLE(2, 3, 0, 1));
	s0 = _mm_add_ps(s0, s1);
  #endif
	_mm_store_ss(&r, s0);
	for ( ; n < N; n++)
		r += x[n] * y[n];
#elif defined(XVM_64) && defined(XVM_SSE2)
	assert(x != NULL && ((size_t)x % 16) == 0);
	assert(y != NULL && ((size_t)y % 16) == 0);
	size_t n, d = N % 4;
	__m128d s0 = _mm_setzero_pd();
	__m128d s1 = _mm_setzero_pd();
	for (n = 0; n < N - d; n += 4) {
		const __m128d x0 = _mm_load_pd(x + n    );
		const __m128d x1 = _mm_load_pd(x + n + 2);
		const __m128d y0 = _mm_load_pd(y + n    );
		const __m128d y1 = _mm_load_pd(y + n + 2);
		const __m128d r0 = _mm_mul_pd(x0, y0);
		const __m128d r1 = _mm_mul_pd(x1, y1);
		s0 = _mm_add_pd(s0, r0);
		s1 = _mm_add_pd(s1, r1);
	}
	s0 = _mm_add_pd(s0, s1);
	s1 = _mm_shuffle_pd(s0, s0, _MM_SHUFFLE2(1, 1));
	s0 = _mm_add_pd(s0, s1);
	_mm_store_sd(&r, s0);
	for ( ; n < N; n++)
		r += x[n] * y[n];
#else
	for (size_t n = 0; n < N; n++)
		r += x[n] * y[n];
#endif
	return r;
}

/* xvm_axpy:
 *   Return the sum of x scaled by a and y:
 *       r = a * x + y
 */
void xvm_axpy(real r[], real a, const real x[], const real y[], size_t N) {
#if defined(XVM_32) && defined(XVM_SSE1)
	assert(r != NULL && ((size_t)r % 16) == 0);
	assert(x != NULL && ((size_t)x % 16) == 0);
	assert(y != NULL && ((size_t)y % 16) == 0);
	const __m128 va = _mm_set1_ps(a);
	for (size_t n = 0; n < N; n += 8) {
		const __m128 x0 = _mm_load_ps(x + n    );
		const __m128 x1 = _mm_load_ps(x + n + 4);
		const __m128 y0 = _mm_load_ps(y + n    );
		const __m128 y1 = _mm_load_ps(y + n + 4);
		const __m128 t0 = _mm_mul_ps(x0, va);
		const __m128 t1 = _mm_mul_ps(x1, va);
		const __m128 r0 = _mm_add_ps(t0, y0);
		const __m128 r1 = _mm_add_ps(t1, y1);
		_mm_store_ps(r + n,     r0);
		_mm_store_ps(r + n + 4, r1);
	}
#elif defined(XVM_64) && defined(XVM_SSE2)
	assert(r != NULL && ((size_t)r % 16) == 0);
	assert(x != NULL && ((size_t)x % 16) == 0);
	assert(y != NULL && ((size_t)y % 16) == 0);
	const __m128d va = _mm_set1_pd(a);
	for (size_t n = 0; n < N; n += 4) {
		const __m128d x0 = _mm_load_pd(x + n    );
		const __m128d x1 = _mm_load_pd(x + n + 2);
		const __m128d y0 = _mm_load_pd(y + n    );
		const __m128d y1 = _mm_load_pd(y + n + 2);
		const __m128d t0 = _mm_mul_pd(x0, va);
		const __m128d t1 = _mm_mul_pd(x1, va);
		const __m128d r0 = _mm_add_pd(t0, y0);
		const __m128d r1 = _mm_add_pd(t1, y1);
		_mm_store_pd(r + n,     r0);
		_mm_store_pd(r + n + 2, r1);
	}
#else
	for (size_t n = 0; n < N; n++)
		r[n] = a * x[n] + y[n];
#endif
}

/* vms_expma:
 *   Compute the component-wise exponential minus <a>:
 *       r[i] <-- e^x[i] - a
 *
 *   The following comments apply to the SSE2 version of this code:
 *
 *   Computation is done four doubles as a time by doing computation in paralell
 *   on two vectors of two doubles using SSE2 intrisics.  If size is not a
 *   multiple of 4, the remaining elements are computed using the stdlib exp().
 *
 *   The computation is done by first doing a range reduction of the argument of
 *   the type e^x = 2^k * e^f choosing k and f so that f is in [-0.5, 0.5].
 *   Then 2^k can be computed exactly using bit operations to build the double
 *   result and e^f can be efficiently computed with enough precision using a
 *   polynomial approximation.
 *
 *   The polynomial approximation is done with 11th order polynomial computed by
 *   Remez algorithm with the Solya suite, instead of the more classical Pade
 *   polynomial form cause it is better suited to parallel execution. In order
 *   to achieve the same precision, a Pade form seems to require three less
 *   multiplications but need a very costly division, so it will be less
 *   efficient.
 *
 *   The maximum error is less than 1lsb and special cases are correctly
 *   handled:
 *     +inf or +oor  -->   return +inf
 *     -inf or -oor  -->   return  0.0
 *     qNaN or sNaN  -->   return qNaN
 *
 *   This code is copyright 2004-2010 Thomas Lavergne and licenced under the
 *   BSD licence like the remaining of Wapiti.
 */
void xvm_expma(real r[], const real x[], real a, size_t N) {
#if defined(XVM_64) && defined(XVM_SSE2)
  #define xvm_vconst(v) (_mm_castsi128_pd(_mm_set1_epi64x((v))))
	assert(r != NULL && ((size_t)r % 16) == 0);
	assert(x != NULL && ((size_t)x % 16) == 0);
	const __m128i vl  = _mm_set1_epi64x(0x3ff0000000000000ULL);
	const __m128d ehi = xvm_vconst(0x4086232bdd7abcd2ULL);
	const __m128d elo = xvm_vconst(0xc086232bdd7abcd2ULL);
	const __m128d l2e = xvm_vconst(0x3ff71547652b82feULL);
	const __m128d hal = xvm_vconst(0x3fe0000000000000ULL);
	const __m128d nan = xvm_vconst(0xfff8000000000000ULL);
	const __m128d inf = xvm_vconst(0x7ff0000000000000ULL);
	const __m128d c1  = xvm_vconst(0x3fe62e4000000000ULL);
	const __m128d c2  = xvm_vconst(0x3eb7f7d1cf79abcaULL);
	const __m128d p0  = xvm_vconst(0x3feffffffffffffeULL);
	const __m128d p1  = xvm_vconst(0x3ff000000000000bULL);
	const __m128d p2  = xvm_vconst(0x3fe0000000000256ULL);
	const __m128d p3  = xvm_vconst(0x3fc5555555553a2aULL);
	const __m128d p4  = xvm_vconst(0x3fa55555554e57d3ULL);
	const __m128d p5  = xvm_vconst(0x3f81111111362f4fULL);
	const __m128d p6  = xvm_vconst(0x3f56c16c25f3bae1ULL);
	const __m128d p7  = xvm_vconst(0x3f2a019fc9310c33ULL);
	const __m128d p8  = xvm_vconst(0x3efa01825f3cb28bULL);
	const __m128d p9  = xvm_vconst(0x3ec71e2bd880fdd8ULL);
	const __m128d p10 = xvm_vconst(0x3e9299068168ac8fULL);
	const __m128d p11 = xvm_vconst(0x3e5ac52350b60b19ULL);
	const __m128d va  = _mm_set1_pd(a);
	for (size_t n = 0; n < N; n += 4) {
		__m128d mn1, mn2, mi1, mi2;
		__m128d t1,  t2,  d1,  d2;
		__m128d v1,  v2,  w1,  w2;
		__m128i k1,  k2;
		__m128d f1,  f2;
		// Load the next four values
		__m128d x1 = _mm_load_pd(x + n    );
		__m128d x2 = _mm_load_pd(x + n + 2);
		// Check for out of ranges, infinites and NaN
		mn1 = _mm_cmpneq_pd(x1, x1);	mn2 = _mm_cmpneq_pd(x2, x2);
		mi1 = _mm_cmpgt_pd(x1, ehi);	mi2 = _mm_cmpgt_pd(x2, ehi);
		x1  = _mm_max_pd(x1, elo);	x2  = _mm_max_pd(x2, elo);
		// Range reduction: we search k and f such that e^x = 2^k * e^f
		// with f in [-0.5, 0.5]
		t1  = _mm_mul_pd(x1, l2e);	t2  = _mm_mul_pd(x2, l2e);
		t1  = _mm_add_pd(t1, hal);	t2  = _mm_add_pd(t2, hal);
		k1  = _mm_cvttpd_epi32(t1);	k2  = _mm_cvttpd_epi32(t2);
		d1  = _mm_cvtepi32_pd(k1);	d2  = _mm_cvtepi32_pd(k2);
		t1  = _mm_mul_pd(d1, c1);	t2  = _mm_mul_pd(d2, c1);
		f1  = _mm_sub_pd(x1, t1);	f2  = _mm_sub_pd(x2, t2);
		t1  = _mm_mul_pd(d1, c2);	t2  = _mm_mul_pd(d2, c2);
		f1  = _mm_sub_pd(f1, t1);	f2  = _mm_sub_pd(f2, t2);
		// Evaluation of e^f using a 11th order polynom in Horner form
		v1  = _mm_mul_pd(f1, p11);	v2  = _mm_mul_pd(f2, p11);
		v1  = _mm_add_pd(v1, p10);	v2  = _mm_add_pd(v2, p10);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p9);	v2  = _mm_add_pd(v2, p9);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p8);	v2  = _mm_add_pd(v2, p8);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p7);	v2  = _mm_add_pd(v2, p7);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p6);	v2  = _mm_add_pd(v2, p6);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p5);	v2  = _mm_add_pd(v2, p5);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p4);	v2  = _mm_add_pd(v2, p4);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p3);	v2  = _mm_add_pd(v2, p3);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p2);	v2  = _mm_add_pd(v2, p2);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p1);	v2  = _mm_add_pd(v2, p1);
		v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
		v1  = _mm_add_pd(v1, p0);	v2  = _mm_add_pd(v2, p0);
		// Evaluation of 2^k using bitops to achieve exact computation
		k1  = _mm_slli_epi32(k1, 20);	k2  = _mm_slli_epi32(k2, 20);
		k1  = _mm_shuffle_epi32(k1, 0x72);
		k2  = _mm_shuffle_epi32(k2, 0x72);
		k1  = _mm_add_epi32(k1, vl);	k2  = _mm_add_epi32(k2, vl);
		w1  = _mm_castsi128_pd(k1);	w2  = _mm_castsi128_pd(k2);
		// Return to full range to substract <a>
	        v1  = _mm_mul_pd(v1, w1);	v2  = _mm_mul_pd(v2, w2);
		v1  = _mm_sub_pd(v1, va);	v2  = _mm_sub_pd(v2, va);
		// Finally apply infinite and NaN where needed
		v1  = _mm_or_pd(_mm_and_pd(mi1, inf), _mm_andnot_pd(mi1, v1));
		v2  = _mm_or_pd(_mm_and_pd(mi2, inf), _mm_andnot_pd(mi2, v2));
		v1  = _mm_or_pd(_mm_and_pd(mn1, nan), _mm_andnot_pd(mn1, v1));
		v2  = _mm_or_pd(_mm_and_pd(mn2, nan), _mm_andnot_pd(mn2, v2));
		// Store the results
		_mm_store_pd(r + n,     v1);
		_mm_store_pd(r + n + 2, v2);
	}
#else
	for (size_t n = 0; n < N; n++)
		r[n] = exp(x[n]) - a;
#endif
}

