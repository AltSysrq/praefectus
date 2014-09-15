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
#include <stdlib.h>
#include <string.h>

#include "outbox.h"

typedef struct {
  praef_hlmsg msg;
  unsigned refcount;
  unsigned char data[FLEXIBLE_ARRAY_MEMBER];
} praef_rc_hlmsg;

typedef struct {
  unsigned queued_at;
  praef_rc_hlmsg* msg;
} praef_mq_entry;

struct praef_mq_s {
  praef_outbox* outbox;
  praef_message_bus* bus;
  const PraefNetworkIdentifierPair_t* unicast;
  int triangular;
  praef_mq_entry* pending;
  unsigned pending_ix, pending_used, pending_cap;
  praef_instant threshold;

  SLIST_ENTRY(praef_mq_s) next;
};

struct praef_outbox_s {
  SLIST_HEAD(,praef_mq_s) subscribers;

  praef_hlmsg_encoder* enc;
  unsigned mtu;
  praef_instant now;
  praef_rc_hlmsg* next;
};

static int praef_mq_enqueue(praef_mq*, praef_rc_hlmsg*);

static praef_rc_hlmsg* praef_rc_hlmsg_new(unsigned mtu) {
  praef_rc_hlmsg* this = malloc(offsetof(praef_rc_hlmsg, data) + mtu + 1);
  if (!this) return NULL;

  this->refcount = 1;
  this->msg.size = mtu+1;
  this->msg.data = this->data;
  return this;
}

static inline void praef_rc_hlmsg_incref(praef_rc_hlmsg* this) {
  ++this->refcount;
}

static inline void praef_rc_hlmsg_decref(praef_rc_hlmsg* this) {
  if (!--this->refcount) free(this);
}

praef_outbox* praef_outbox_new(praef_hlmsg_encoder* enc,
                               unsigned mtu) {
  praef_outbox* this;

  if (!enc) return NULL;

  this = malloc(sizeof(praef_outbox));
  if (!this) {
    praef_hlmsg_encoder_delete(enc);
    return NULL;
  }

  this->enc = enc;
  this->mtu = mtu;
  this->now = 0;
  SLIST_INIT(&this->subscribers);

  this->next = praef_rc_hlmsg_new(mtu);
  if (!this->next) {
    free(this);
    praef_hlmsg_encoder_delete(enc);
    return NULL;
  }

  return this;
}

void praef_outbox_delete(praef_outbox* this) {
  assert(SLIST_EMPTY(&this->subscribers));
  praef_hlmsg_encoder_delete(this->enc);
  if (this->next) free(this->next);
  free(this);
}

int praef_outbox_append(praef_outbox* this, const PraefMsg_t* msg) {
  praef_mq* mq;
  int ok = 1;

  /* next will be NULL if a prior oom occurred */
  if (!this->next) return 0;

  if (praef_hlmsg_encoder_append(&this->next->msg, this->enc, msg)) {
    SLIST_FOREACH(mq, &this->subscribers, next)
      ok &= praef_mq_enqueue(mq, this->next);

    praef_rc_hlmsg_decref(this->next);
    this->next = praef_rc_hlmsg_new(this->mtu);
  }

  return ok && this->next;
}

int praef_outbox_append_singleton(praef_outbox* this, const PraefMsg_t* msg) {
  praef_rc_hlmsg* next;
  praef_mq* mq;
  int ok = 1;

  next = praef_rc_hlmsg_new(this->mtu);
  if (!next) return 0;

  praef_hlmsg_encoder_singleton(&next->msg, this->enc, msg);
  SLIST_FOREACH(mq, &this->subscribers, next)
    ok &= praef_mq_enqueue(mq, next);

  praef_rc_hlmsg_decref(next);

  return ok;
}

int praef_outbox_flush(praef_outbox* this) {
  praef_mq* mq;
  int ok = 1;

  if (!this->next) return 0;

  if (praef_hlmsg_encoder_flush(&this->next->msg, this->enc)) {
    SLIST_FOREACH(mq, &this->subscribers, next)
      ok &= praef_mq_enqueue(mq, this->next);

    praef_rc_hlmsg_decref(this->next);
    this->next = praef_rc_hlmsg_new(this->mtu);
  }

  return ok && this->next;
}

praef_instant praef_outbox_get_now(const praef_outbox* this) {
  return praef_hlmsg_encoder_get_now(this->enc);
}

void praef_outbox_set_now(praef_outbox* this, praef_instant now) {
  praef_hlmsg_encoder_set_now(this->enc, now);
  this->now = now;
}

praef_mq* praef_mq_new(praef_outbox* outbox,
                       praef_message_bus* bus,
                       const PraefNetworkIdentifierPair_t* unicast) {
  praef_mq* this = malloc(sizeof(praef_mq));
  if (!this) return NULL;

  this->outbox = outbox;
  this->bus = bus;
  this->unicast = unicast;
  this->triangular = 0;
  this->pending_ix = 0;
  this->pending_used = 0;
  this->pending_cap = 16;
  this->threshold = ~0u;

  this->pending = calloc(this->pending_cap, sizeof(praef_mq_entry));
  if (!this->pending) {
    free(this);
    return NULL;
  }

  SLIST_INSERT_HEAD(&outbox->subscribers, this, next);
  return this;
}

void praef_mq_delete(praef_mq* this) {
  unsigned i;

  for (i = 0; i < this->pending_cap; ++i)
    if (this->pending[i].msg)
      praef_rc_hlmsg_decref(this->pending[i].msg);

  SLIST_REMOVE(&this->outbox->subscribers, this, praef_mq_s, next);
  free(this->pending);
  free(this);
}

void praef_mq_set_threshold(praef_mq* this, praef_instant threshold) {
  this->threshold = threshold;
}

void praef_mq_set_triangular(praef_mq* this, int triangular) {
  this->triangular = triangular;
}

static int praef_mq_enqueue(praef_mq* this, praef_rc_hlmsg* msg) {
  praef_mq_entry* new_entries;

  if (this->pending_used == this->pending_cap) {
    new_entries = realloc(this->pending,
                          2 * this->pending_cap * sizeof(praef_mq_entry));
    if (!new_entries) return 0;

    memset(new_entries + this->pending_cap, 0,
           this->pending_cap * sizeof(praef_mq_entry));
    this->pending = new_entries;
    this->pending_ix = this->pending_cap;
    this->pending_cap *= 2;
  }

  while (this->pending[this->pending_ix].msg) {
    ++this->pending_ix;
    this->pending_ix &= (this->pending_cap - 1);
  }

  praef_rc_hlmsg_incref(msg);
  this->pending[this->pending_ix].msg = msg;
  this->pending[this->pending_ix].queued_at = this->outbox->now;
  ++this->pending_used;
  return 1;
}

void praef_mq_update(praef_mq* this) {
  unsigned i;

  for (i = 0; i < this->pending_cap; ++i) {
    if (this->pending[i].msg &&
        this->pending[i].queued_at <= this->threshold) {
      if (this->unicast && this->triangular)
        (*this->bus->triangular_unicast)(
          this->bus, this->unicast,
          this->pending[i].msg->msg.data,
          this->pending[i].msg->msg.size-1);
      else if (this->unicast)
        (*this->bus->unicast)(
          this->bus, this->unicast,
          this->pending[i].msg->msg.data,
          this->pending[i].msg->msg.size-1);
      else
        (*this->bus->broadcast)(this->bus,
                                this->pending[i].msg->msg.data,
                                this->pending[i].msg->msg.size-1);

      praef_rc_hlmsg_decref(this->pending[i].msg);
      this->pending[i].msg = NULL;
      --this->pending_used;
    }
  }
}
