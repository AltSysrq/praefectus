/*-
 * Copyright (c) 2013, 2014 Jason Lingle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef FRAC_H_
#define FRAC_H_

/**
 * A fraction represents essentially a cached division, and thus ranges from
 * 0..1 (similar to a zo_scaling_factor). Multiplications with a fraction are
 * much faster than a division by the original denominator, at the cost of some
 * precision.
 */
typedef unsigned fraction;
#define FRACTION_BITS 31
#define FRACTION_BASE (((fraction)1) << FRACTION_BITS)

#define fraction_of(denom)                                      \
  ((fraction)(FRACTION_BASE / ((unsigned)(denom))))

static inline fraction fraction_of2(unsigned numerator, unsigned denominator) {
  unsigned long long tmp = numerator;
  tmp *= FRACTION_BASE;
  tmp /= denominator;
  return tmp;
}

static inline unsigned fraction_umul(unsigned numerator, fraction mult) {
  unsigned long long tmp64, mult64;
  mult64 = mult;
  tmp64 = numerator;
  tmp64 *= mult64;
  return tmp64 >> FRACTION_BITS;
}

static inline signed fraction_smul(signed numerator, fraction mult) {
  signed long long tmp64, mult64;
  mult64 = (signed long long)(unsigned long long)/*zext*/ mult;
  tmp64 = numerator;
  tmp64 *= mult64;
  return tmp64 >> FRACTION_BITS;
}

/**
 * precise_fractions are similar to fractions, and generally follow the same
 * usage pattern. Differences are as follows:
 *
 * - They are far more expensive on 32-bit platforms.
 * - Values passed into the multiply functions must not have more than 16
 *   non-sign bits.
 * - The multiply functions return an *intermediate* 32-bit result, which must
 *   be reduced back to 16-bit with the reduce functions. This allows
 *   performing some other compuation on the more precise values first.
 */
typedef unsigned long long precise_fraction;
#define PRECISE_FRACTION_BITS 47
#define PRECISE_FRACTION_BASE (((precise_fraction)1) << PRECISE_FRACTION_BITS)
#define PRECISE_FRACTION_INTERMEDIATE_TRAILING_BITS 16

static inline precise_fraction precise_fraction_of(unsigned long long denom) {
  return PRECISE_FRACTION_BASE / denom;
}

static inline unsigned precise_fraction_umul(unsigned numerator,
                                             precise_fraction denom) {
  unsigned long long m64;
  m64 = numerator;
  m64 *= denom;
  return m64 >> (PRECISE_FRACTION_BITS -
                 PRECISE_FRACTION_INTERMEDIATE_TRAILING_BITS);
}

static inline signed precise_fraction_smul(signed numerator,
                                           precise_fraction denom) {
  signed long long m64;
  m64 = numerator;
  m64 *= denom;
  return m64 >> (PRECISE_FRACTION_BITS -
                 PRECISE_FRACTION_INTERMEDIATE_TRAILING_BITS);
}

static inline unsigned precise_fraction_ured(unsigned v) {
  return v >> PRECISE_FRACTION_INTERMEDIATE_TRAILING_BITS;
}

static inline signed precise_fraction_sred(signed v) {
  return v >> PRECISE_FRACTION_INTERMEDIATE_TRAILING_BITS;
}

/**
 * Inverts the effect of reduction, creating an intermediate value without
 * multiplying.
 */
static inline unsigned precise_fraction_uexp(unsigned v) {
  return v << PRECISE_FRACTION_INTERMEDIATE_TRAILING_BITS;
}

static inline signed precise_fraction_sexp(signed v) {
  return v << PRECISE_FRACTION_INTERMEDIATE_TRAILING_BITS;
}

/**
 * Multiplies two precise_fractions. This loses some precision, but has no
 * problems with larger values.
 */
static inline precise_fraction precise_fraction_fmul(
  precise_fraction a,
  precise_fraction b)
{
  a >>= PRECISE_FRACTION_BITS/2;
  b >>= PRECISE_FRACTION_BITS - PRECISE_FRACTION_BITS/2;
  return a*b;
}

#endif /* FRAC_H_ */
