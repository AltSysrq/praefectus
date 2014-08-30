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

#include <libpraefectus/dsa.h>

static praef_signator* signators[4];
static praef_verifier* verifier;

defsuite(libpraefectus_dsa);

defsetup {
  unsigned i;

  for (i = 0; i < 4; ++i)
    signators[i] = praef_signator_new();

  verifier = praef_verifier_new();
}

defteardown {
  unsigned i;

  for (i = 0; i < 4; ++i)
    praef_signator_delete(signators[i]);

  praef_verifier_delete(verifier);
}

deftest(can_recognise_own_signature) {
  unsigned char signature[PRAEF_SIGNATURE_SIZE];
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];

  praef_signator_pubkey(pubkey, signators[0]);
  ck_assert(praef_verifier_assoc(verifier, pubkey, 1));
  ck_assert(praef_verifier_is_assoc(verifier, pubkey));

  praef_signator_sign(signature, signators[0], pubkey, sizeof(pubkey));
  ck_assert_int_eq(1, praef_verifier_verify(
                     verifier,
                     praef_signator_pubkey_hint(signators[0]),
                     signature,
                     pubkey, sizeof(pubkey)));
}

deftest(rejects_corrupted_message) {
  unsigned char signature[PRAEF_SIGNATURE_SIZE];
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];

  praef_signator_pubkey(pubkey, signators[0]);
  ck_assert(praef_verifier_assoc(verifier, pubkey, 1));

  praef_signator_sign(signature, signators[0], pubkey, sizeof(pubkey));
  pubkey[0] ^= 0x01;
  ck_assert_int_eq(0, praef_verifier_verify(
                     verifier,
                     praef_signator_pubkey_hint(signators[0]),
                     signature,
                     pubkey, sizeof(pubkey)));
}

deftest(rejects_corrupted_signature) {
  unsigned char signature[PRAEF_SIGNATURE_SIZE];
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];

  praef_signator_pubkey(pubkey, signators[0]);
  ck_assert(praef_verifier_assoc(verifier, pubkey, 1));

  memset(signature, 0, sizeof(signature));
  ck_assert_int_eq(0, praef_verifier_verify(
                     verifier,
                     praef_signator_pubkey_hint(signators[0]),
                     signature,
                     pubkey, sizeof(pubkey)));
}

deftest(rejects_invalid_hint_even_if_signature_valid) {
  praef_signator* signator = NULL;
  unsigned char signature[PRAEF_SIGNATURE_SIZE];
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];
  unsigned i;

  /* Find a signator with a non-zero hint.
   * Note that this gives this test a 1/2**64 chance of failing randomly, not
   * that anyone will ever run into that case.
   */
  for (i = 0; i < 4; ++i)
    if (0 != praef_signator_pubkey_hint(signators[i]))
      signator = signators[i];

  ck_assert_ptr_ne(NULL, signator);

  praef_signator_pubkey(pubkey, signator);
  ck_assert(praef_verifier_assoc(verifier, pubkey, 1));

  praef_signator_sign(signature, signator, pubkey, sizeof(pubkey));
  ck_assert_int_eq(0, praef_verifier_verify(
                     verifier,
                     0,
                     signature,
                     pubkey, sizeof(pubkey)));
}

deftest(correctly_identifies_message_origin) {
  unsigned char signature[PRAEF_SIGNATURE_SIZE];
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];
  unsigned i;

  for (i = 0; i < 4; ++i) {
    praef_signator_pubkey(pubkey, signators[i]);
    ck_assert(praef_verifier_assoc(verifier, pubkey, i+1));
  }

  for (i = 0; i < 4; ++i) {
    praef_signator_sign(signature, signators[i], pubkey, sizeof(pubkey));
    ck_assert_int_eq(i+1, praef_verifier_verify(
                       verifier,
                       praef_signator_pubkey_hint(signators[i]),
                       signature,
                       pubkey, sizeof(pubkey)));
  }
}

deftest(rejects_signator_with_duplicate_key) {
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];

  praef_signator_pubkey(pubkey, signators[0]);
  ck_assert(praef_verifier_assoc(verifier, pubkey, 1));
  ck_assert(!praef_verifier_assoc(verifier, pubkey, 2));
}

deftest(can_remove_signator) {
  unsigned char signature[PRAEF_SIGNATURE_SIZE];
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];
  unsigned i;

  for (i = 0; i < 4; ++i) {
    praef_signator_pubkey(pubkey, signators[i]);
    ck_assert(praef_verifier_assoc(verifier, pubkey, i+1));
  }

  ck_assert(praef_verifier_disassoc(verifier, pubkey));
  ck_assert(!praef_verifier_is_assoc(verifier, pubkey));
  for (i = 0; i < 4; ++i) {
    praef_signator_sign(signature, signators[i], pubkey, sizeof(pubkey));
    ck_assert_int_eq(3 == i? 0 : i+1, praef_verifier_verify(
                       verifier,
                       praef_signator_pubkey_hint(signators[i]),
                       signature,
                       pubkey, sizeof(pubkey)));
  }
}

deftest(cannot_remove_same_signator_more_than_once) {
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];

  praef_signator_pubkey(pubkey, signators[0]);
  ck_assert(praef_verifier_assoc(verifier, pubkey, 1));
  ck_assert(praef_verifier_disassoc(verifier, pubkey));
  ck_assert(!praef_verifier_disassoc(verifier, pubkey));
}

deftest(can_differentiate_between_signators_with_same_pubkey_hint) {
  /* Since there isn't really any control over the public keys (and, in any
   * case, they run through SHA-3 first), perform a birthday attach to force
   * this 16-bit field to collide.
   */
  static praef_signator* sigs[65536] = { 0 };
  praef_signator* collided, * collidee, * signator;
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];
  unsigned char signature[PRAEF_SIGNATURE_SIZE];
  praef_pubkey_hint hint;
  unsigned i;

  for (;;) {
    signator = praef_signator_new();
    hint = praef_signator_pubkey_hint(signator);
    if (sigs[hint]) {
      collided = signator;
      collidee = sigs[hint];
      break;
    } else {
      sigs[hint] = signator;
    }
  }

  /* Sanity checks */
  ck_assert_int_eq(praef_signator_pubkey_hint(collided),
                   praef_signator_pubkey_hint(collidee));
  ck_assert_int_eq(hint, praef_signator_pubkey_hint(collided));

  praef_signator_pubkey(pubkey, collided);
  ck_assert(praef_verifier_assoc(verifier, pubkey, 1));
  praef_signator_pubkey(pubkey, collidee);
  ck_assert(praef_verifier_assoc(verifier, pubkey, 2));

  praef_signator_sign(signature, collided, pubkey, sizeof(pubkey));
  ck_assert_int_eq(1, praef_verifier_verify(verifier, hint,
                                            signature,
                                            pubkey, sizeof(pubkey)));
  praef_signator_sign(signature, collidee, pubkey, sizeof(pubkey));
  ck_assert_int_eq(2, praef_verifier_verify(verifier, hint,
                                            signature,
                                            pubkey, sizeof(pubkey)));

  praef_signator_delete(collided);
  for (i = 0; i < 256; ++i)
    if (sigs[i])
      praef_signator_delete(sigs[i]);
}
