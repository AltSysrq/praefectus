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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "keccak.h"
#include "messages/PraefMsg.h"
#include "common.h"
#include "secure-random.h"
#include "hl-msg.h"

#define PRAEF_HLMSG_PUBKEY_HINT_OFF 0
#define PRAEF_HLMSG_PUBKEY_HINT_SZ 2
#define PRAEF_HLMSG_SIGNATURE_OFF                               \
  (PRAEF_HLMSG_PUBKEY_HINT_OFF + PRAEF_HLMSG_PUBKEY_HINT_SZ)
#define PRAEF_HLMSG_SIGNATURE_SZ PRAEF_SIGNATURE_SIZE
#define PRAEF_HLMSG_FLAGS_OFF                                   \
  (PRAEF_HLMSG_SIGNATURE_OFF + PRAEF_HLMSG_SIGNATURE_SZ)
#define PRAEF_HLMSG_FLAGS_SZ 1
#define PRAEF_HLMSG_INSTANT_OFF                         \
  (PRAEF_HLMSG_FLAGS_OFF + PRAEF_HLMSG_FLAGS_SZ)
#define PRAEF_HLMSG_INSTANT_SZ 4
#define PRAEF_HLMSG_SERNO_OFF                           \
  (PRAEF_HLMSG_INSTANT_OFF + PRAEF_HLMSG_INSTANT_SZ)
#define PRAEF_HLMSG_SERNO_SZ 4
#define PRAEF_HLMSG_SEGMENT_OFF                         \
  (PRAEF_HLMSG_SERNO_OFF + PRAEF_HLMSG_SERNO_SZ)

struct praef_hlmsg_encoder_s {
  praef_hlmsg_type_flag type;
  praef_signator* signator;
  size_t mtu;
  size_t append_garbage;
  size_t garbage_bytes;
  praef_instant now;
  praef_advisory_serial_number* serno;

  praef_advisory_serial_number private_serno;

  /* Arrays of size garbage_bytes, which are part of the same memory
   * allocation as the encoder itself. Each message (if garbage_bytes is
   * non-zero), garbage_salt and garbage are fed into Keccak, and the output
   * overwrites garbage. Each is initialised with securely random data.
   */
  unsigned char* garbage_salt, * garbage;

  /* Message into which results are accumulated. The data pointer points within
   * the same memory allocation containing the encoder itself. The size field
   * tracks the current *actual* size of the message, not including the
   * append_garbage to be added in later on.
   */
  praef_hlmsg msg;
};

static praef_hlmsg_type_flag praef_hlmsg_type_flag_for(PraefMsg_PR);
static void praef_hlmsg_encoder_init_msg(praef_hlmsg*, praef_hlmsg_encoder*);
static void praef_hlmsg_encoder_finish_msg(praef_hlmsg*, praef_hlmsg_encoder*);
static unsigned praef_per_encode(void* dst, size_t max,
                                 const PraefMsg_t*);

int praef_hlmsg_is_valid(const praef_hlmsg* message) {
  const unsigned char* data = message->data;
  unsigned offset;
  praef_hlmsg_type_flag expected_type;
  PraefMsg_t deserialised, * deserialised_ptr = & deserialised;;
  asn_dec_rval_t decode_result;
  int ok;

  /* Needs to have at least the base header, plus one segment (which cannot be
   * empty).
   */
  if (message->size < PRAEF_HLMSG_SEGMENT_OFF + 3)
    return 0;

  /* The flags must be valid */
  if (data[PRAEF_HLMSG_FLAGS_OFF] > 2)
    return 0;

  /* Safe to read the expected type */
  expected_type = praef_hlmsg_type(message);

  /* There must be at least one real segment */
  if (0 == data[PRAEF_HLMSG_SEGMENT_OFF])
    return 0;

  /* The segments must not go beyond the end of the buffer.
   * Look for an explicit 0-termination (provided by the "garbage after this
   * byte" marker or the 0 explicitly added in the wrapped form) immediately
   * after a segment. Stop if the offset winds up outside the buffer; in this
   * case, the hlmsg is invalid.
   */
  offset = PRAEF_HLMSG_SEGMENT_OFF;
  while (offset < message->size && data[offset])
    offset += data[offset] + 1;
  if (offset >= message->size) return 0;

  /* Ensure each embedded message can be decoded and is of the appropriate
   * type.
   */
  for (offset = PRAEF_HLMSG_SEGMENT_OFF;
       offset < message->size && data[offset];
       offset += data[offset] + 1) {
    memset(&deserialised, 0, sizeof(deserialised));
    decode_result = uper_decode_complete(
      NULL, &asn_DEF_PraefMsg, (void**)&deserialised_ptr,
      data+offset+1, data[offset]);

    ok = (RC_OK == decode_result.code);
    if (ok)
      ok = (expected_type == praef_hlmsg_type_flag_for(deserialised.present));
    if (ok)
      ok = !(*asn_DEF_PraefMsg.check_constraints)(
        &asn_DEF_PraefMsg, &deserialised, NULL, NULL);

    (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, &deserialised, 1);

    if (!ok) return 0;
  }

  /* Everything is valid */
  return 1;
}

praef_pubkey_hint praef_hlmsg_pubkey_hint(const praef_hlmsg* message) {
  const unsigned char* data = message->data;
  data += PRAEF_HLMSG_PUBKEY_HINT_OFF;

  return data[0] | (((praef_pubkey_hint)data[1]) << 8);
}

unsigned char* praef_hlmsg_signature(const praef_hlmsg* message) {
  const unsigned char* data = message->data;
  data += PRAEF_HLMSG_SIGNATURE_OFF;

  return (unsigned char*)data;
}

praef_hlmsg_type_flag praef_hlmsg_type(const praef_hlmsg* message) {
  const unsigned char* data = message->data;

  switch (data[PRAEF_HLMSG_FLAGS_OFF] & 0x3) {
  case 0: return praef_htf_committed_redistributable;
  case 1: return praef_htf_uncommitted_redistributable;
  case 2: return praef_htf_rpc_type;
  /* Else, the message in invalid, which is against this function's
   * contract.
   */
  default: abort();
  }
}

praef_instant praef_hlmsg_instant(const praef_hlmsg* message) {
  const unsigned char* data = message->data;
  data += PRAEF_HLMSG_INSTANT_OFF;

  return
    (((praef_instant)data[0]) <<  0) |
    (((praef_instant)data[1]) <<  8) |
    (((praef_instant)data[2]) << 16) |
    (((praef_instant)data[3]) << 24);
}

praef_advisory_serial_number praef_hlmsg_serno(const praef_hlmsg* message) {
  const unsigned char* data = message->data;
  data += PRAEF_HLMSG_SERNO_OFF;

  return
    (((praef_advisory_serial_number)data[0]) <<  0) |
    (((praef_advisory_serial_number)data[1]) <<  8) |
    (((praef_advisory_serial_number)data[2]) << 16) |
    (((praef_advisory_serial_number)data[3]) << 24);
}

const void* praef_hlmsg_signable(const praef_hlmsg* message) {
  const unsigned char* data = message->data;

  return data + PRAEF_HLMSG_FLAGS_OFF;
}

size_t praef_hlmsg_signable_sz(const praef_hlmsg* message) {
  return message->size - PRAEF_HLMSG_FLAGS_OFF - 1;
}

const praef_hlmsg_segment* praef_hlmsg_first(const praef_hlmsg* message) {
  const unsigned char* data = message->data;

  return (const praef_hlmsg_segment*)(data + PRAEF_HLMSG_SEGMENT_OFF);
}

const praef_hlmsg_segment* praef_hlmsg_snext(const praef_hlmsg_segment* seg) {
  const unsigned char* data = (const unsigned char*)seg;
  data += *data + 1;

  if (*data)
    return (const praef_hlmsg_segment*)data;
  else
    return NULL;
}

PraefMsg_t* praef_hlmsg_sdec(const praef_hlmsg_segment* segment) {
  PraefMsg_t* result;
  const unsigned char* data = (const unsigned char*)segment;
  asn_dec_rval_t decode_result;

  decode_result = uper_decode_complete(
    NULL, &asn_DEF_PraefMsg, (void**)&result,
    data+1, *data);

  if (RC_OK != decode_result.code) {
    (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, result, 0);
    return NULL;
  }

  return result;
}

praef_hlmsg* praef_hlmsg_of(const void* data, size_t sz) {
  praef_hlmsg* message;

  message = malloc(sizeof(praef_hlmsg) + sz + 1);
  if (!message) return NULL;

  message->size = sz + 1;
  message->data = message + 1;
  memcpy((void*)message->data, data, sz);
  ((unsigned char*)(void*)message->data)[sz] = 0;
  return message;
}

praef_hlmsg_encoder* praef_hlmsg_encoder_new(praef_hlmsg_type_flag type,
                                             praef_signator* signator,
                                             praef_advisory_serial_number* sn,
                                             size_t mtu,
                                             size_t append_garbage) {
  praef_hlmsg_encoder* this;
  unsigned char* base;
  size_t garbage_bytes = append_garbage? append_garbage-1 : 0;

  if (1 == append_garbage ||
      mtu < PRAEF_HLMSG_MTU_MIN + append_garbage)
    return NULL;

  base = malloc(sizeof(praef_hlmsg_encoder) +
                2 * garbage_bytes +
                mtu + 1);
  if (!base) return NULL;

  this = (praef_hlmsg_encoder*)base;
  this->type = type;
  this->signator = signator;
  this->mtu = mtu;
  this->append_garbage = append_garbage;
  this->garbage_bytes = garbage_bytes;
  this->now = 0;
  this->serno = sn? sn : &this->private_serno;
  this->private_serno = 0;
  this->garbage_salt = base + sizeof(praef_hlmsg_encoder);
  this->garbage = this->garbage_salt + garbage_bytes;
  this->msg.size = 0;
  this->msg.data = this->garbage + garbage_bytes;

  if (garbage_bytes) {
    /* garbage_salt and garbage are contiguous, so read both in one call. */
    if (!praef_secure_random(this->garbage_salt, 2 * garbage_bytes)) {
      free(this);
      return NULL;
    }
  }

  return this;
}

void praef_hlmsg_encoder_delete(praef_hlmsg_encoder* this) {
  free(this);
}

void praef_hlmsg_encoder_set_now(praef_hlmsg_encoder* this, praef_instant now) {
  assert(0 == this->msg.size);
  this->now = now;
}

static unsigned praef_per_encode(void* dst, size_t max,
                                 const PraefMsg_t* message) {
  asn_enc_rval_t encode_result;

  encode_result = uper_encode_to_buffer(
    &asn_DEF_PraefMsg, (PraefMsg_t*)message, dst, max);
  if (-1 == encode_result.encoded) {
    fprintf(stderr, "libpraefectus: fatal: PER encoder returned failure\n");
    abort();
  }

  return (encode_result.encoded + 7) / 8;
}

int praef_hlmsg_encoder_append(praef_hlmsg* dst,
                               praef_hlmsg_encoder* this,
                               const PraefMsg_t* message) {
  unsigned char serialised[255];
  unsigned num_bytes;
  unsigned char* data;
  int flushed = 0;

  assert(dst->size >= this->mtu + 1);
  assert(this->type == praef_hlmsg_type_flag_for(message->present));

  num_bytes = praef_per_encode(serialised, sizeof(serialised), message);
  if (num_bytes + 1 + this->msg.size + this->append_garbage > this->mtu) {
    flushed = praef_hlmsg_encoder_flush(dst, this);
    assert(flushed);
  }

  if (!this->msg.size)
    praef_hlmsg_encoder_init_msg(&this->msg, this);

  data = (void*)this->msg.data;
  data += this->msg.size;
  *data = num_bytes;
  memcpy(data + 1, serialised, num_bytes);
  this->msg.size += num_bytes + 1;
  return flushed;
}

void praef_hlmsg_encoder_singleton(praef_hlmsg* dst,
                                   praef_hlmsg_encoder* this,
                                   const PraefMsg_t* message) {
  unsigned char serialised[255];
  unsigned num_bytes;
  unsigned char* data;

  assert(dst->size >= this->mtu + 1);
  assert(this->type == praef_hlmsg_type_flag_for(message->present));

  num_bytes = praef_per_encode(serialised, sizeof(serialised), message);

  praef_hlmsg_encoder_init_msg(dst, this);
  data = (void*)dst->data;
  data += dst->size;
  *data = num_bytes;
  memcpy(data + 1, serialised, num_bytes);
  dst->size += num_bytes + 1;

  praef_hlmsg_encoder_finish_msg(dst, this);
}

int praef_hlmsg_encoder_flush(praef_hlmsg* dst,
                              praef_hlmsg_encoder* this) {
  assert(dst->size >= this->mtu + 1);

  if (!this->msg.size) return 0;

  praef_hlmsg_encoder_finish_msg(&this->msg, this);
  dst->size = this->msg.size;
  memcpy((unsigned char*)dst->data, this->msg.data, this->msg.size);
  this->msg.size = 0;
  return 1;
}

static praef_hlmsg_type_flag praef_hlmsg_type_flag_for(PraefMsg_PR present) {
  switch (present) {
  case PraefMsg_PR_ping:
  case PraefMsg_PR_pong:
  case PraefMsg_PR_getnetinfo:
  case PraefMsg_PR_netinfo:
  case PraefMsg_PR_joinreq:
  case PraefMsg_PR_htls:
  case PraefMsg_PR_htdir:
  case PraefMsg_PR_htread:
  case PraefMsg_PR_htrange:
  case PraefMsg_PR_appuni:
  case PraefMsg_PR_received:
  case PraefMsg_PR_jointree:
  case PraefMsg_PR_jtentry:
    return praef_htf_rpc_type;

  case PraefMsg_PR_endorsement:
  case PraefMsg_PR_commandeer:
  case PraefMsg_PR_commit:
  case PraefMsg_PR_route:
    return praef_htf_uncommitted_redistributable;

  case PraefMsg_PR_chmod:
  case PraefMsg_PR_appevt:
  case PraefMsg_PR_vote:
    return praef_htf_committed_redistributable;

  /* Not a default case so that the compiler can warn us if we forget
   * something.
   */
  case PraefMsg_PR_NOTHING: abort();
  }

  /* unreachable */
  abort();
}

static void praef_hlmsg_encoder_init_msg(praef_hlmsg* message,
                                         praef_hlmsg_encoder* this) {
  praef_pubkey_hint pubkey_hint;
  unsigned char* data = (void*)message->data;

  if (this->signator) {
    pubkey_hint = praef_signator_pubkey_hint(this->signator);
    data[0] = pubkey_hint;
    data[1] = pubkey_hint >> 8;
  }

  switch (this->type) {
  case praef_htf_committed_redistributable:
    data[PRAEF_HLMSG_FLAGS_OFF] = 0;
    break;

  case praef_htf_uncommitted_redistributable:
    data[PRAEF_HLMSG_FLAGS_OFF] = 1;
    break;

  case praef_htf_rpc_type:
    data[PRAEF_HLMSG_FLAGS_OFF] = 2;
    break;
  }

  data[PRAEF_HLMSG_INSTANT_OFF + 0] = this->now >>  0;
  data[PRAEF_HLMSG_INSTANT_OFF + 1] = this->now >>  8;
  data[PRAEF_HLMSG_INSTANT_OFF + 2] = this->now >> 16;
  data[PRAEF_HLMSG_INSTANT_OFF + 3] = this->now >> 24;

  data[PRAEF_HLMSG_SERNO_OFF + 0] = *this->serno >>  0;
  data[PRAEF_HLMSG_SERNO_OFF + 1] = *this->serno >>  8;
  data[PRAEF_HLMSG_SERNO_OFF + 2] = *this->serno >> 16;
  data[PRAEF_HLMSG_SERNO_OFF + 3] = *this->serno >> 24;
  ++*this->serno;

  message->size = PRAEF_HLMSG_SEGMENT_OFF;
}

static void praef_hlmsg_encoder_finish_msg(praef_hlmsg* message,
                                           praef_hlmsg_encoder* this) {
  unsigned char* data = (void*)message->data;
  praef_keccak_sponge sponge;

  if (this->garbage_bytes) {
    praef_keccak_sponge_init(&sponge, PRAEF_KECCAK_RATE, PRAEF_KECCAK_CAP);
    praef_keccak_sponge_absorb(&sponge,
                               this->garbage_salt, this->garbage_bytes);
    praef_keccak_sponge_absorb(&sponge, this->garbage, this->garbage_bytes);
    praef_keccak_sponge_squeeze(&sponge, this->garbage, this->garbage_bytes);

    data[message->size] = 0;
    memcpy(data + message->size + 1, this->garbage, this->garbage_bytes);
    message->size += this->garbage_bytes + 1;
  }

  data[message->size] = 0;
  ++message->size;

  if (this->signator)
    praef_signator_sign(praef_hlmsg_signature(message),
                        this->signator,
                        praef_hlmsg_signable(message),
                        praef_hlmsg_signable_sz(message));
}

