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

#include "test.h"

#include <libpraefectus/dynamic.h>

defsuite(libpraefectus_dynamic);

PRAEF_IVD_DECL(nonexistent);
PRAEF_IVD_DECL(nonexistent); /* ensure duplicatable */
PRAEF_IVD_DECL(poke_all_U);
PRAEF_IVD_DECL(poke_b_U);

typedef struct {
  praef_dynobj self;
  unsigned value;
} object;

static praef_ivdr common_poke_all_U();
static praef_ivdr b_poke_b_U();

static const praef_ivdm a_vtable[] = {
  PRAEF_IVDM(common, poke_all_U),
  PRAEF_IVDM_END
};

static const praef_ivdm b_vtable[] = {
  PRAEF_IVDM(common, poke_all_U),
  PRAEF_IVDM(b, poke_b_U),
  PRAEF_IVDM_END
};

static const praef_ivdm c_vtable[] = {
  PRAEF_IVDM(common, poke_all_U),
  PRAEF_IVDM_END
};

static object a, b, c;
static praef_dynobj_chain chain;

defsetup {
  a.self.vtable = a_vtable;
  a.value = 0;
  b.self.vtable = b_vtable;
  b.value = 0;
  c.self.vtable = c_vtable;
  c.value = 0;

  SLIST_INIT(&chain);
  /* Insert c,b,a so that the final order is a,b,c */
  SLIST_INSERT_HEAD(&chain, &c.self, next);
  SLIST_INSERT_HEAD(&chain, &b.self, next);
  SLIST_INSERT_HEAD(&chain, &a.self, next);
}

defteardown { }

static praef_ivdr common_poke_all_U(object* this, unsigned value) {
  this->value = value;

  return PRAEF_IVD_NEXT(&this, this, praef_ivdr_imp, poke_all_U)(this, value);
}

static praef_ivdr b_poke_b_U(object* this, unsigned value) {
  this->value = value;

  return PRAEF_IVD_NEXT(&this, this, praef_ivdr_imp, poke_b_U)(this, value);
}

deftest(invoking_unimplemented_method_returns_not_imp) {
  void* _;
  ck_assert_int_eq(praef_ivdr_not_imp, PRAEF_IVD(&_, &chain, nonexistent)(_));
}

deftest(can_poke_all) {
  void* _;

  ck_assert_int_eq(praef_ivdr_imp, PRAEF_IVD(&_, &chain, poke_all_U)(_, 42));
  ck_assert_int_eq(42, a.value);
  ck_assert_int_eq(42, b.value);
  ck_assert_int_eq(42, c.value);
}

deftest(can_poke_b) {
  void* _;

  ck_assert_int_eq(praef_ivdr_imp, PRAEF_IVD(&_, &chain, poke_b_U)(_, 42));
  ck_assert_int_eq(0, a.value);
  ck_assert_int_eq(42, b.value);
  ck_assert_int_eq(0, c.value);
}
