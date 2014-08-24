/*-
 * Copyright (c) 2014 Jason Lingle
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

#ifndef TEST_H_
#define TEST_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <check.h>
#include <stdlib.h>

extern const char* suite_names[1024];
extern void (*suite_impls[1024])(Suite*);
extern unsigned suite_num;

static const char* test_names[1024];
static TFun test_impls[1024];
static unsigned test_num;

static void (*setups[256])(void);
static void (*teardowns[256])(void);
static unsigned setup_num, teardown_num;

static inline void run_suite(Suite* suite) {
  unsigned i, j;
  TCase* kase;

  for (i = 0; i < test_num; ++i) {
    /* Run the tests in reverse registration order, since GCC runs the
     * constructors in the file from the bottom up.
     */
    kase = tcase_create(test_names[test_num-i-1]);
    for (j = 0; j < setup_num && j < teardown_num; ++j)
      tcase_add_checked_fixture(kase, setups[j], teardowns[j]);
    tcase_add_test(kase, test_impls[test_num-i-1]);
    suite_add_tcase(suite, kase);
  }
}

#define defsuite(name)                          \
  static void _register_suite_##name(void)      \
    __attribute__((constructor));               \
  static void _register_suite_##name(void) {    \
    suite_names[suite_num] = #name;             \
    suite_impls[suite_num] = run_suite;         \
    ++suite_num;                                \
  }                                             \
  void dummy()

#define deftest(name)                           \
  static void _register_##name(void)            \
    __attribute__((constructor));               \
  static void name##_impl(void);                \
  START_TEST(name) {                            \
    name##_impl();                              \
  }                                             \
  END_TEST                                      \
  static void _register_##name(void) {          \
    test_names[test_num] = #name;               \
    test_impls[test_num] = name;                \
    ++test_num;                                 \
  }                                             \
  static void name##_impl(void)

#define GLUE(a,b) _GLUE(a,b)
#define _GLUE(a,b) a##b
#define defsetup                                        \
  static void GLUE(_setup_,__LINE__)(void);             \
  static void GLUE(_registersetup_,__LINE__)(void)      \
    __attribute__((constructor));                       \
  static void GLUE(_registersetup_,__LINE__)(void) {    \
    setups[setup_num++] = GLUE(_setup_,__LINE__);       \
  }                                                     \
  static void GLUE(_setup_,__LINE__)(void)

#define defteardown                                        \
  static void GLUE(_teardown_,__LINE__)(void);             \
  static void GLUE(_registerteardown_,__LINE__)(void)      \
    __attribute__((constructor));                          \
  static void GLUE(_registerteardown_,__LINE__)(void) {    \
    teardowns[teardown_num++] = GLUE(_teardown_,__LINE__); \
  }                                                        \
  static void GLUE(_teardown_,__LINE__)(void)

#define ANONYMOUS GLUE(_anon_,__LINE__)
#define _ID(...) __VA_ARGS__
#define STRIP_PARENS(x) _ID(_ID x)
/* Yes, we're using K&R style declarations right in the middle of C99 with a
 * bunch of GNU extensions. It's seemingly the cleanest way to do lambda() with
 * more than one parameter.
 *
 * To be honest, I was kind of suprised that GCC actually accepts this syntax
 * in this context.
 */
#define lambda(arg_names, args, expr)                   \
  ({typeof(({args; expr;}))                             \
  GLUE(_lambda_,ANONYMOUS) arg_names args; {            \
      return ({expr;});                                 \
    }                                                   \
  GLUE(_lambda_,ANONYMOUS); })
#define lambdav(args, expr)                     \
  ({void GLUE(_lambdav_,ANONYMOUS) args {       \
      expr;                                     \
    }                                           \
    GLUE(_lambdav_,ANONYMOUS); })

#endif /* TEST_H_ */
