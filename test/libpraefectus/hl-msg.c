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

#include <string.h>

#include <libpraefectus/hl-msg.h>
#include <libpraefectus/dsa.h>

/* The tests in this file are fairly strongly dependent on the high-level
 * message data format, which makes them a bit fragile.
 */

defsuite(libpraefectus_hlmsg);

static praef_hlmsg_encoder* encoder;
static praef_signator* signator;
static praef_verifier* verifier;
static praef_hlmsg* msg;
static PraefMsg_t* submsg;

static void free_submsg(PraefMsg_t* m) {
  (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, m, 0);
}

defsetup {
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];
  signator = praef_signator_new();
  verifier = praef_verifier_new();
  praef_signator_pubkey(pubkey, signator);
  praef_verifier_assoc(verifier, pubkey, 1);
  encoder = NULL;
  msg = NULL;
  submsg = NULL;
}

defteardown {
  if (encoder) praef_hlmsg_encoder_delete(encoder);
  free(msg);
  free_submsg(submsg);
  praef_signator_delete(signator);
  praef_verifier_delete(verifier);
}

#define SIGNABLE_OFF 34
#define FLAG_OFF SIGNABLE_OFF

#define INSTANT_1234 0x04, 0x03, 0x02, 0x01
#define SERNO_5678 0x08, 0x07, 0x06, 0x05
#define SIGNATURE \
  0, 0, 0, 0, 0, 0, 0, 0, \
  0, 0, 0, 0, 0, 0, 0, 0, \
  0, 0, 0, 0, 0, 0, 0, 0, \
  0, 0, 0, 0, 0, 0, 0, 0

#define GENERIC_HEADER 0xEF, 0xBE, SIGNATURE, 0, INSTANT_1234, SERNO_5678

static void mk_appuni_msg(const void* data, size_t sz) {
  free_submsg(submsg);

  submsg = calloc(1, sizeof(PraefMsg_t));
  submsg->present = PraefMsg_PR_appuni;
  OCTET_STRING_fromBuf(&submsg->choice.appuni.data, data, sz);
}

static void mk_appuni_msg_str(const char* str) {
  mk_appuni_msg(str, strlen(str));
}

deftest(decodes_pubkey_hint) {
  unsigned char data[] = {
    GENERIC_HEADER,
    0x01, 0x00,
    0x00
  };
  praef_hlmsg msg = {
    .size = sizeof(data),
    .data = data
  };

  ck_assert_int_eq(0xBEEF, (unsigned)praef_hlmsg_pubkey_hint(&msg));
}

deftest(returns_pointer_to_signature) {
  unsigned char data[] = {
    GENERIC_HEADER,
    0x01, 0x00,
    0x00
  };
  unsigned char signature[] = { SIGNATURE };
  praef_hlmsg msg = {
    .size = sizeof(data),
    .data = data
  };

  ck_assert_int_eq(0, memcmp(signature, praef_hlmsg_signature(&msg),
                             sizeof(signature)));
}

deftest(decodes_type) {
  unsigned char data[] = {
    GENERIC_HEADER,
    0x01, 0x00,
    0x00
  };
  praef_hlmsg msg = {
    .size = sizeof(data),
    .data = data
  };

  ck_assert_int_eq(praef_htf_committed_redistributable,
                   praef_hlmsg_type(&msg));

  data[FLAG_OFF] = 1;
  ck_assert_int_eq(praef_htf_uncommitted_redistributable,
                   praef_hlmsg_type(&msg));
  data[FLAG_OFF] = 2;
  ck_assert_int_eq(praef_htf_rpc_type,
                   praef_hlmsg_type(&msg));
}

deftest(decodes_instant) {
  unsigned char data[] = {
    GENERIC_HEADER,
    0x01, 0x00,
    0x00
  };
  praef_hlmsg msg = {
    .size = sizeof(data),
    .data = data
  };

  ck_assert_int_eq(0x01020304, praef_hlmsg_instant(&msg));
}

deftest(decodes_serno) {
  unsigned char data[] = {
    GENERIC_HEADER,
    0x01, 0x00,
    0x00
  };
  praef_hlmsg msg = {
    .size = sizeof(data),
    .data = data
  };

  ck_assert_int_eq(0x05060708, praef_hlmsg_serno(&msg));
}

deftest(identifies_signable_region) {
  unsigned char data[] = {
    GENERIC_HEADER,
    0x01, 0x00,
    0x00
  };
  praef_hlmsg msg = {
    .size = sizeof(data),
    .data = data
  };

  ck_assert_ptr_eq(data + SIGNABLE_OFF, praef_hlmsg_signable(&msg));
  ck_assert_int_eq(sizeof(data) - SIGNABLE_OFF - 1, praef_hlmsg_signable_sz(&msg));
}

#define C(ptr) ((unsigned char*)(ptr))

deftest(traverses_segments) {
  unsigned char data[] = {
    GENERIC_HEADER,
    0x02, 0xCA, 0xFE,
    0x03, 0xC0, 0xDE, 0x01,
    0x00, 0xDE, 0xAD,
    0x00
  };
  unsigned char c_02cafe[] = {
    0x02, 0xCA, 0xFE
  };
  unsigned char c_03c0de01[] = {
    0x03, 0xC0, 0xDE, 0x01
  };
  praef_hlmsg msg = {
    .size = sizeof(data),
    .data = data
  };
  const praef_hlmsg_segment* seg;

  seg = praef_hlmsg_first(&msg);
  ck_assert_ptr_ne(NULL, seg);
  ck_assert_int_eq(0, memcmp(c_02cafe, seg, sizeof(c_02cafe)));

  seg = praef_hlmsg_snext(seg);
  ck_assert_ptr_ne(NULL, seg);
  ck_assert_int_eq(0, memcmp(c_03c0de01, seg, sizeof(c_03c0de01)));

  seg = praef_hlmsg_snext(seg);
  ck_assert_ptr_eq(NULL, seg);
}

deftest(can_correctly_wrap_byte_array) {
  unsigned char data[] = { 1, 2 };

  msg = praef_hlmsg_of(data, sizeof(data));
  ck_assert_int_eq(3, msg->size);
  ck_assert_int_eq(1, C(msg->data)[0]);
  ck_assert_int_eq(2, C(msg->data)[1]);
  ck_assert_int_eq(0, C(msg->data)[2]);
  ck_assert_ptr_ne(data, msg->data);
}

deftest(validation_rejects_truncated_before_flags) {
  unsigned char data[] = { 0, 0, SIGNATURE };

  msg = praef_hlmsg_of(data, sizeof(data)-1);
  ck_assert(!praef_hlmsg_is_valid(msg));
}

deftest(validation_rejects_with_no_segments) {
  unsigned char data[] = {
    GENERIC_HEADER
  };

  msg = praef_hlmsg_of(data, sizeof(data));
  ck_assert(!praef_hlmsg_is_valid(msg));
}

deftest(validation_rejects_zero_segment_message) {
  unsigned char data[] = {
    GENERIC_HEADER,
    0, 0xDE, 0xAD
  };

  msg = praef_hlmsg_of(data, sizeof(data));
  ck_assert(!praef_hlmsg_is_valid(msg));
}

deftest(validation_rejects_invalid_flags) {
  unsigned char data[] = {
    0, 0, SIGNATURE,
    3, INSTANT_1234, SERNO_5678,
    1, 1
  };

  msg = praef_hlmsg_of(data, sizeof(data));
  ck_assert(!praef_hlmsg_is_valid(msg));
}

deftest(validation_rejects_oversized_segment) {
  unsigned char data[] = {
    GENERIC_HEADER,
    1
  };

  msg = praef_hlmsg_of(data, sizeof(data));
  ck_assert(!praef_hlmsg_is_valid(msg));
}

deftest(validation_rejects_unparsable_message) {
  unsigned char data[] = {
    GENERIC_HEADER,
    1, 0xFF
  };

  msg = praef_hlmsg_of(data, sizeof(data));
  ck_assert(!praef_hlmsg_is_valid(msg));
}

deftest(can_encode_valid_singleton_messages) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg msg = { .size = sizeof(data), .data = data };

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, NULL, NULL, PRAEF_HLMSG_MTU_MIN, 0);
  mk_appuni_msg_str("hello world");
  praef_hlmsg_encoder_singleton(&msg, encoder, submsg);

  ck_assert_int_lt(msg.size, sizeof(data));
  ck_assert(praef_hlmsg_is_valid(&msg));
}

deftest(can_encode_valid_composite_messages) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg msg = { .size = sizeof(data), .data = data };

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, NULL, NULL, PRAEF_HLMSG_MTU_MIN, 0);
  mk_appuni_msg_str("hello world");
  ck_assert(!praef_hlmsg_encoder_append(&msg, encoder, submsg));
  ck_assert(!praef_hlmsg_encoder_append(&msg, encoder, submsg));
  ck_assert(praef_hlmsg_encoder_flush(&msg, encoder));

  ck_assert_int_lt(msg.size, sizeof(data));
  ck_assert(praef_hlmsg_is_valid(&msg));
}

deftest(validation_rejects_message_with_incorrect_type) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg msg = { .size = sizeof(data), .data = data };

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, NULL, NULL, PRAEF_HLMSG_MTU_MIN, 0);
  mk_appuni_msg_str("hello world");
  praef_hlmsg_encoder_singleton(&msg, encoder, submsg);

  ck_assert(praef_hlmsg_is_valid(&msg));
  data[FLAG_OFF] = 0;
  ck_assert(!praef_hlmsg_is_valid(&msg));
}

deftest(validation_rejects_truncated_message) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg msg = { .size = sizeof(data), .data = data };
  praef_hlmsg* truncated;

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, NULL, NULL, PRAEF_HLMSG_MTU_MIN, 0);
  mk_appuni_msg_str("hello world");
  praef_hlmsg_encoder_singleton(&msg, encoder, submsg);

  truncated = praef_hlmsg_of(msg.data, msg.size - 2);
  ck_assert(praef_hlmsg_is_valid(&msg));
  ck_assert(!praef_hlmsg_is_valid(truncated));

  free(truncated);
}

deftest(validation_rejects_oversized_JoinAccept) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1] = { 0 };
  praef_hlmsg msg = { .size = sizeof(data), .data = data };
  PraefMsg_t accept = {
    .present = PraefMsg_PR_accept,
    .choice = {
      .accept = {
        .signature = { 0 },
        .request = {
          .auth = NULL,
          .publickey = { 0 },
          .identifier = {
            .internet = NULL,
            .intranet = {
              .port = 31337,
              .address = {
                .present = PraefIpAddress_PR_ipv4,
                .choice = {
                  .ipv4 = { 0 },
                },
              },
            },
          },
        },
      },
    },
  };

  OCTET_STRING_fromBuf(&accept.choice.accept.signature,
                       (char*)data, PRAEF_SIGNATURE_SIZE);
  OCTET_STRING_fromBuf(&accept.choice.accept
                       .request.publickey,
                       (char*)data, PRAEF_PUBKEY_SIZE);
  OCTET_STRING_fromBuf(&accept.choice.accept
                       .request.identifier.intranet
                       .address.choice.ipv4,
                       (char*)data, 4);

  encoder = praef_hlmsg_encoder_new(
    praef_htf_uncommitted_redistributable, NULL, NULL, PRAEF_HLMSG_MTU_MIN, 0);
  while (!praef_hlmsg_encoder_append(&msg, encoder, &accept));

  ck_assert_int_gt(msg.size, 129);
  ck_assert(!praef_hlmsg_is_valid(&msg));

  (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, &accept, 1);
}

deftest(doesnt_overflow_message_data_array) {
  /* This requires valgrind to test correctly. */
  unsigned char* data;
  unsigned char scratch[200] = { 0 };
  praef_hlmsg msg;
  unsigned second_size;
  int had_exact_match = 0;

  data = malloc(PRAEF_HLMSG_MTU_MIN+1);
  msg.data = data;
  msg.size = PRAEF_HLMSG_MTU_MIN+1;

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, NULL, NULL, PRAEF_HLMSG_MTU_MIN, 0);

  for (second_size = 1; second_size <= sizeof(scratch); ++second_size) {
    msg.size = PRAEF_HLMSG_MTU_MIN+1;
    mk_appuni_msg(scratch, sizeof(scratch));
    ck_assert(!praef_hlmsg_encoder_append(&msg, encoder, submsg));
    mk_appuni_msg(scratch, second_size);
    if (praef_hlmsg_encoder_append(&msg, encoder, submsg))
        goto validate_result;

    ck_assert(praef_hlmsg_encoder_flush(&msg, encoder));
    if (msg.size == PRAEF_HLMSG_MTU_MIN+1) {
      ck_assert(!had_exact_match);
      had_exact_match = 1;
    }
  }

  ck_abort();

  validate_result:
  ck_assert(had_exact_match);
  ck_assert_int_lt(msg.size, PRAEF_HLMSG_MTU_MIN-2);

  msg.size = PRAEF_HLMSG_MTU_MIN + 1;
  ck_assert(praef_hlmsg_encoder_flush(&msg, encoder));
  msg.size = PRAEF_HLMSG_MTU_MIN + 1;
  ck_assert(!praef_hlmsg_encoder_flush(&msg, encoder));

  free(data);
}

deftest(encoding_is_idempotent_with_signator_and_no_garbage_bytes) {
  unsigned char data[2][PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg msg;
  size_t first_size;
  praef_advisory_serial_number serno = 0;

  memset(data[0], 0, PRAEF_HLMSG_MTU_MIN+1);
  memset(data[1], ~0, PRAEF_HLMSG_MTU_MIN+1);

  mk_appuni_msg_str("hello world");

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, signator, &serno, PRAEF_HLMSG_MTU_MIN, 0);
  msg.data = data[0];
  msg.size = PRAEF_HLMSG_MTU_MIN+1;
  praef_hlmsg_encoder_singleton(&msg, encoder, submsg);
  first_size = msg.size;

  serno = 0;
  msg.data = data[1];
  msg.size = PRAEF_HLMSG_MTU_MIN+1;
  praef_hlmsg_encoder_singleton(&msg, encoder, submsg);

  ck_assert_int_eq(first_size, msg.size);
  ck_assert_int_eq(0, memcmp(data[0], data[1], first_size));
}

deftest(encoding_is_nonidempotent_with_garbage_bytes) {
  unsigned char data[2][PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg msg;
  size_t first_size;
  praef_advisory_serial_number serno = 0;

  memset(data[0], 0, PRAEF_HLMSG_MTU_MIN+1+8);
  memset(data[1], 0, PRAEF_HLMSG_MTU_MIN+1+8);

  mk_appuni_msg_str("hello world");

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, signator, &serno, PRAEF_HLMSG_MTU_MIN+8, 8);
  msg.data = data[0];
  msg.size = PRAEF_HLMSG_MTU_MIN+1+8;
  praef_hlmsg_encoder_singleton(&msg, encoder, submsg);
  first_size = msg.size;

  serno = 0;
  msg.data = data[1];
  msg.size = PRAEF_HLMSG_MTU_MIN+1+8;
  praef_hlmsg_encoder_singleton(&msg, encoder, submsg);

  ck_assert_int_eq(first_size, msg.size);
  ck_assert_int_ne(0, memcmp(data[0], data[1], first_size));
}

deftest(produces_message_with_valid_signature) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1+8];
  praef_hlmsg msg = { .data = data, .size = sizeof(data) };

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, signator, NULL, PRAEF_HLMSG_MTU_MIN+8, 8);
  mk_appuni_msg_str("hello world");
  praef_hlmsg_encoder_singleton(&msg, encoder, submsg);

  ck_assert_int_eq(1, praef_verifier_verify(
                     verifier,
                     praef_hlmsg_pubkey_hint(&msg),
                     praef_hlmsg_signature(&msg),
                     praef_hlmsg_signable(&msg),
                     praef_hlmsg_signable_sz(&msg)));
}

deftest(flipping_any_bit_invalidates_signature) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1+8];
  praef_hlmsg msg = { .data = data, .size = sizeof(data) };
  unsigned byte, bit;

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, signator, NULL, PRAEF_HLMSG_MTU_MIN+8, 8);
  mk_appuni_msg_str("hello world");
  praef_hlmsg_encoder_singleton(&msg, encoder, submsg);

  for (byte = 0; byte < msg.size - 1; ++byte) {
    for (bit = 0; bit < 8; ++bit) {
      data[byte] ^= (1 << bit);
      ck_assert_int_eq(0, praef_verifier_verify(
                         verifier,
                         praef_hlmsg_pubkey_hint(&msg),
                         praef_hlmsg_signature(&msg),
                         praef_hlmsg_signable(&msg),
                         praef_hlmsg_signable_sz(&msg)));
      data[byte] ^= (1 << bit);
    }
  }
}

deftest(private_serial_number_increments_per_hlmsg) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg msg = { .data = data, .size = sizeof(data) };
  unsigned i;

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, NULL, NULL, PRAEF_HLMSG_MTU_MIN, 0);
  mk_appuni_msg_str("foo");

  for (i = 0; i < 8; ++i) {
    msg.size = sizeof(data);
    ck_assert(!praef_hlmsg_encoder_append(&msg, encoder, submsg));
    ck_assert(!praef_hlmsg_encoder_append(&msg, encoder, submsg));
    ck_assert(praef_hlmsg_encoder_flush(&msg, encoder));

    ck_assert_int_eq(i, praef_hlmsg_serno(&msg));
  }
}

deftest(shared_serial_number_increments_shared_value_per_hlmsg) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg msg = { .data = data, .size = sizeof(data) };
  praef_advisory_serial_number serno;

  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, NULL, &serno, PRAEF_HLMSG_MTU_MIN, 0);
  mk_appuni_msg_str("foo");

  serno = 42;
  ck_assert(!praef_hlmsg_encoder_append(&msg, encoder, submsg));
  ck_assert(!praef_hlmsg_encoder_append(&msg, encoder, submsg));
  ck_assert(praef_hlmsg_encoder_flush(&msg, encoder));
  ck_assert_int_eq(43, serno);
  ck_assert_int_eq(42, praef_hlmsg_serno(&msg));
}

deftest(maximal_join_accept_fits_into_join_tree_entry) {
  PraefMsg_t ja, jte;
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1], garbage[64];
  unsigned char data2[PRAEF_HLMSG_MTU_MIN+1];
  OCTET_STRING_t auth = { .buf = garbage, .size = 26 };
  OCTET_STRING_t jtd;
  praef_hlmsg msg = { .data = data, .size = sizeof(data) };
  praef_hlmsg msg2 = { .data = data2, .size = sizeof(data2) };

  encoder = praef_hlmsg_encoder_new(
    praef_htf_uncommitted_redistributable,
    NULL, NULL, PRAEF_HLMSG_MTU_MIN, 0);
  memset(&ja, 0, sizeof(ja));
  ja.present = PraefMsg_PR_accept;
  ja.choice.accept.request.publickey.buf = garbage;
  ja.choice.accept.request.publickey.size = PRAEF_PUBKEY_SIZE;
  ja.choice.accept.request.identifier.intranet.port = 0x8000;
  ja.choice.accept.request.identifier.intranet.address.present =
    PraefIpAddress_PR_ipv6;
  ja.choice.accept.request.identifier.intranet.address.choice.ipv6.buf =
    garbage;
  ja.choice.accept.request.identifier.intranet.address.choice.ipv6.size = 32;
  ja.choice.accept.request.identifier.internet =
    &ja.choice.accept.request.identifier.intranet;
  ja.choice.accept.request.auth = &auth;
  ja.choice.accept.instant = 0x80000000;
  ja.choice.accept.signature.buf = garbage;
  ja.choice.accept.signature.size = PRAEF_SIGNATURE_SIZE;

  praef_hlmsg_encoder_singleton(&msg, encoder, &ja);
  ck_assert_int_le(msg.size-1, PRAEF_HLMSG_JOINACCEPT_MAX);

  praef_hlmsg_encoder_delete(encoder);
  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type,
    NULL, NULL, PRAEF_HLMSG_MTU_MIN, 0);
  memset(&jte, 0, sizeof(jte));
  jte.present = PraefMsg_PR_jtentry;
  jte.choice.jtentry.node = 0x80000000;
  jte.choice.jtentry.offset = 0x80000000;
  jte.choice.jtentry.nkeys = 0x80000000;
  jte.choice.jtentry.data = &jtd;
  jtd.buf = data;
  jtd.size = msg.size-1;
  /* This call will abort if the resulting encoded form is > 255 bytes */
  praef_hlmsg_encoder_singleton(&msg2, encoder, &jte);
}
