/*-
 * Copyright (c) 2016, Jason Lingle
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

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "bsd.h"
#include "system.h"
#include "stdsys.h"
#include "simple.h"

typedef struct praef_simple_object_s {
  praef_object header;
  praef_simple_drop_t drop;
  praef_simple_object_step_t step;
  praef_simple_object_rewind_t rewind;
  SLIST_ENTRY(praef_simple_object_s) next;
} praef_simple_object;

typedef struct praef_simple_event_s {
  praef_event header;
  praef_simple_drop_t drop;
  praef_simple_event_apply_t apply;
} praef_simple_event;

struct praef_simple_context_s {
  praef_app* bridge;
  praef_system* system;
  praef_std_state stack;
  void* userdata;

  SLIST_HEAD(, praef_simple_object_s) objects;

  praef_simple_cb_create_node_object_t create_node_object;
  size_t object_size;

  praef_simple_cb_decode_event_t decode_event;
  size_t event_size;

  praef_simple_cb_auth_is_valid_t auth_is_valid;
  praef_simple_cb_auth_gen_t auth_gen;

  praef_simple_cb_permit_object_id_t permit_object_id;
  praef_simple_cb_acquire_id_t acquire_id;
  praef_simple_cb_discover_node_t discover_node;
  praef_simple_cb_remove_node_t remove_node;
  praef_simple_cb_join_tree_traversed_t join_tree_traversed;
  praef_simple_cb_ht_scan_progress_t ht_scan_progress;
  praef_simple_cb_awaiting_stability_t awaiting_stability;
  praef_simple_cb_information_complete_t information_complete;
  praef_simple_cb_clock_synced_t clock_synced;
  praef_simple_cb_gained_grant_t gained_grant;
  praef_simple_cb_recv_unicast_t recv_unicast;
  praef_simple_cb_log_t log;

  praef_simple_cb_event_optimism_t event_optimism;
  praef_simple_cb_event_vote_t event_vote;
};

static void praef_simple_flatten_ip_address(
  praef_simple_ip_address* dst,
  const PraefIpAddress_t* src);
static void praef_simple_flatten_netid(
  praef_simple_netid* dst,
  const PraefNetworkIdentifier_t* src);
static void praef_simple_flatten_netid_pair(
  praef_simple_netid_pair* dst,
  const PraefNetworkIdentifierPair_t* src);
static void praef_simple_flatten_join_request(
  praef_simple_join_request* dst,
  const PraefMsgJoinRequest_t* src);

static void praef_simple_impl_create_node_object(
  praef_app* bridge, praef_object_id id);
static praef_event* praef_simple_impl_decode_event(
  praef_app* bridge, praef_instant instant, praef_object_id object,
  praef_event_serial_number serno,
  const void* data, size_t sz);
static void praef_simple_impl_object_step(
  praef_object* obj, praef_userdata ud);
static void praef_simple_impl_object_rewind(
  praef_object* obj, praef_instant instant);
static void praef_simple_impl_event_apply(
  praef_object* obj, const praef_event* evt, praef_userdata ud);
static void praef_simple_impl_event_free(void* vevt);
static int praef_simple_impl_is_auth_valid(
  praef_app* bridge, const PraefMsgJoinRequest_t* request);
static void praef_simple_impl_gen_auth(
  praef_app* bridge, PraefMsgJoinRequest_t* request,
  OCTET_STRING_t* auth_data);

static unsigned praef_simple_impl_optimistic_events(
  praef_app* bridge, const praef_event* vevt);
static int praef_simple_impl_event_vote(
  praef_app* bridge, const praef_event* vevt, praef_userdata vud);

praef_simple_context* praef_simple_new(
  praef_userdata userdata,
  praef_message_bus* bus,
  const PraefNetworkIdentifierPair_t* self,
  unsigned std_latency,
  praef_system_profile profile,
  praef_system_ip_version ip_version,
  praef_system_network_locality net_locality,
  unsigned mtu
) {
  praef_simple_context* this = NULL;

  this = calloc(1, sizeof(praef_simple_context));
  if (!this) goto fail;
  SLIST_INIT(&this->objects);
  if (!praef_std_state_init(&this->stack)) goto fail;
  if (!(this->bridge = praef_stdsys_new(&this->stack))) goto fail;
  if (!(this->system = praef_system_new(
          this->bridge, bus, self,
          std_latency, profile, ip_version, net_locality, mtu)))
    goto fail;
  praef_stdsys_set_system(this->bridge, this->system);
  praef_stdsys_set_userdata(this->bridge, this);

  this->bridge->create_node_object =
    praef_simple_impl_create_node_object;
  this->bridge->decode_event = praef_simple_impl_decode_event;

  return this;

  fail:
  if (this) {
    praef_simple_delete(this);
  }
  return NULL;
}

void praef_simple_delete(praef_simple_context* this) {
  if (this->system) praef_system_delete(this->system);
  if (this->bridge) praef_stdsys_delete(this->bridge);
  praef_std_state_cleanup(&this->stack);
  free(this);
}

praef_system* praef_simple_get_system(const praef_simple_context* this) {
  return this->system;
}

void* praef_simple_get_userdata(const praef_simple_context* this) {
  return this->userdata;
}

void praef_simple_cb_create_node_object(
  praef_simple_context* this,
  praef_simple_cb_create_node_object_t callback,
  size_t object_size
) {
  this->create_node_object = callback;
  this->object_size = object_size;
}

static void praef_simple_impl_create_node_object(
  praef_app* bridge, praef_object_id id
) {
  praef_simple_context* this;
  praef_simple_object* obj;

  this = praef_stdsys_userdata(bridge);
  if (!this->create_node_object) {
    praef_system_oom(this->system);
    return;
  }

  obj = malloc(sizeof(praef_simple_object) + this->object_size);
  if (!obj) {
    praef_system_oom(this->system);
    return;
  }

  obj->header.step = praef_simple_impl_object_step;
  obj->header.rewind = praef_simple_impl_object_rewind;
  obj->header.id = id;
  if (!(*this->create_node_object)(
        obj + 1, &obj->drop, &obj->step, &obj->rewind,
        this, id)) {
    free(this);
    praef_system_oom(this->system);
    return;
  }

  SLIST_INSERT_HEAD(&this->objects, obj, next);
  praef_context_add_object(this->stack.context, (praef_object*)obj);
}

static void praef_simple_impl_object_step(
  praef_object* vobj, praef_userdata vud
) {
  praef_simple_object* obj = (praef_simple_object*)vobj;
  praef_simple_context* this = vud;

  (*obj->step)(obj + 1, obj->header.id, this);
}

static void praef_simple_impl_object_rewind(
  praef_object* vobj, praef_instant instant
) {
  praef_simple_object* obj = (praef_simple_object*)vobj;

  (*obj->rewind)(obj + 1, obj->header.id, instant);
}

void praef_simple_cb_decode_event(
  praef_simple_context* this,
  praef_simple_cb_decode_event_t decode_event,
  size_t event_size
) {
  this->decode_event = decode_event;
  this->event_size = event_size;
}

static praef_event* praef_simple_impl_decode_event(
  praef_app* bridge, praef_instant instant, praef_object_id object,
  praef_event_serial_number serno,
  const void* data, size_t sz
) {
  praef_simple_context* this;
  praef_simple_event* evt;

  this = praef_stdsys_userdata(bridge);
  if (!this->decode_event) {
    praef_system_oom(this->system);
    return NULL;
  }

  evt = malloc(sizeof(praef_simple_event) + this->event_size);
  if (!evt) {
    praef_system_oom(this->system);
    return NULL;
  }

  evt->header.apply = praef_simple_impl_event_apply;
  evt->header.free = praef_simple_impl_event_free;
  evt->header.object = object;
  evt->header.instant = instant;
  evt->header.serial_number = serno;

  if (!(*this->decode_event)(
        evt + 1, &evt->drop, &evt->apply,
        this, instant, object, serno, data, sz)) {
    free(evt);
    return NULL;
  }

  return (praef_event*)evt;
}

static void praef_simple_impl_event_free(void* vevt) {
  praef_simple_event* evt = vevt;
  (*evt->drop)(evt + 1);
  free(evt);
}

static void praef_simple_impl_event_apply(
  praef_object* vobject, const praef_event* vevt,
  praef_userdata vud
) {
  praef_simple_object* object = (praef_simple_object*)vobject;
  praef_simple_event* evt = (praef_simple_event*)vevt;
  praef_simple_context* this = vud;

  (*evt->apply)(object + 1, evt + 1, evt->header.object,
                evt->header.instant, evt->header.serial_number,
                this);
}

void praef_simple_cb_auth(
  praef_simple_context* this,
  praef_simple_cb_auth_is_valid_t is_valid,
  praef_simple_cb_auth_gen_t gen
) {
  this->bridge->is_auth_valid_opt = praef_simple_impl_is_auth_valid;
  this->auth_is_valid = is_valid;
  this->bridge->gen_auth_opt = praef_simple_impl_gen_auth;
  this->auth_gen = gen;
}

static int praef_simple_impl_is_auth_valid(
  praef_app* bridge, const PraefMsgJoinRequest_t* request
) {
  praef_simple_context* this;
  praef_simple_join_request flat;

  this = praef_stdsys_userdata(bridge);
  if (!this->auth_is_valid) return 1;

  praef_simple_flatten_join_request(&flat, request);
  return (*this->auth_is_valid)(this, &flat);
}

static void praef_simple_impl_gen_auth(
  praef_app* bridge, PraefMsgJoinRequest_t* request,
  OCTET_STRING_t* auth_data
) {
  praef_simple_context* this;
  praef_simple_join_request flat;

  this = praef_stdsys_userdata(bridge);
  praef_simple_flatten_join_request(&flat, request);
  if (this->auth_gen) (*this->auth_gen)(&flat, this);
  if (flat.auth_size) {
    memcpy(auth_data->buf, flat.auth, flat.auth_size);
    auth_data->size = flat.auth_size;
    request->auth = auth_data;
  }
}

static void praef_simple_flatten_join_request(
  praef_simple_join_request* dst,
  const PraefMsgJoinRequest_t* src
) {
  memcpy(dst->public_key, src->publickey.buf, PRAEF_PUBKEY_SIZE);
  praef_simple_flatten_netid_pair(
    &dst->identifier, &src->identifier);
  if (src->auth) {
    dst->auth_size = src->auth->size;
    memcpy(dst->auth, src->auth->buf, src->auth->size);
  } else {
    dst->auth_size = 0;
  }
}

static void praef_simple_flatten_netid_pair(
  praef_simple_netid_pair* dst,
  const PraefNetworkIdentifierPair_t* src
) {
  dst->global = !!src->internet;
  praef_simple_flatten_netid(&dst->intranet, &src->intranet);
  if (src->internet)
    praef_simple_flatten_netid(&dst->internet, src->internet);
}

static void praef_simple_flatten_netid(
  praef_simple_netid* dst,
  const PraefNetworkIdentifier_t* src
) {
  praef_simple_flatten_ip_address(&dst->address, &src->address);
  dst->port = src->port;
}

static void praef_simple_flatten_ip_address(
  praef_simple_ip_address* dst,
  const PraefIpAddress_t* src
) {
  unsigned i;
  unsigned short p;

  if (PraefIpAddress_PR_ipv4 == src->present) {
    dst->version = 4;
    memcpy(dst->v4, src->choice.ipv4.buf, 4);
  } else {
    for (i = 0; i < 8; ++i) {
      p = src->choice.ipv6.buf[i*2 + 0];
      p <<= 8;
      p |= src->choice.ipv6.buf[i*2 + 1];
      dst->v6[i] = p;
    }
  }
}

#define DELEGATE(name, retty, dflt, retrn, declargs, listargs, extra)   \
  static retty praef_simple_impl_##name declargs;                       \
  void praef_simple_cb_##name(                                          \
    praef_simple_context* this,                                         \
    praef_simple_cb_##name##_t cb                                       \
  ) {                                                                   \
    this->bridge->name##_opt = praef_simple_impl_##name;                \
    this->name = cb;                                                    \
  }                                                                     \
  static retty praef_simple_impl_##name declargs {                      \
    praef_simple_context* this = praef_stdsys_userdata(bridge);         \
    if (!this->name) return dflt;                                       \
    extra                                                               \
    retrn (*this->name) listargs;                                       \
  }

DELEGATE(permit_object_id, int, 1, return,
         (praef_app* bridge, praef_object_id id),
         (this, id),)
DELEGATE(acquire_id, void, , ,
         (praef_app* bridge, praef_object_id id),
         (this, id),)
DELEGATE(discover_node, void, , ,
         (praef_app* bridge, const PraefNetworkIdentifierPair_t* netid,
          praef_object_id node_id),
         (this, &flat_netid, node_id),
         praef_simple_netid_pair flat_netid;
         praef_simple_flatten_netid_pair(&flat_netid, netid);)
DELEGATE(remove_node, void, , ,
         (praef_app* bridge, praef_object_id node_id),
         (this, node_id), )
DELEGATE(join_tree_traversed, void, , ,
         (praef_app* bridge),
         (this), )
DELEGATE(ht_scan_progress, void, , ,
         (praef_app* bridge, unsigned num, unsigned denom),
         (this, num, denom), )
DELEGATE(awaiting_stability, void, , ,
         (praef_app* bridge, praef_object_id node,
          praef_instant systime, praef_instant committed,
          praef_instant validated),
         (this, node, systime, committed, validated), )
DELEGATE(information_complete, void, , ,
         (praef_app* bridge),
         (this), )
DELEGATE(clock_synced, void, , ,
         (praef_app* bridge),
         (this), )
DELEGATE(gained_grant, void, , ,
         (praef_app* bridge),
         (this), )
DELEGATE(recv_unicast, void, , ,
         (praef_app* bridge, praef_object_id from_node,
          praef_instant instant,
          const void* data, size_t size),
         (this, from_node, instant, data, size), )
DELEGATE(log, void, , ,
         (praef_app* bridge, const char* message),
         (this, message), )

#undef DELEGATE

void praef_simple_cb_event_vote(
  praef_simple_context* this,
  praef_simple_cb_event_vote_t cb
) {
  praef_stdsys_event_vote(this->bridge, praef_simple_impl_event_vote);
  this->event_vote = cb;
}

static int praef_simple_impl_event_vote(
  praef_app* bridge, const praef_event* vevt,
  praef_userdata ud
) {
  praef_simple_context* this = ud;
  const praef_simple_event* evt = (const praef_simple_event*)vevt;

  /* Shouldn't happen unless someone explicitly sets NULL */
  if (!this->event_vote) return 1;
  return (*this->event_vote)(
    this, evt->header.object, evt->header.instant,
    evt->header.serial_number, evt + 1);
}

void praef_simple_cb_event_optimism(
  praef_simple_context* this,
  praef_simple_cb_event_optimism_t cb
) {
  praef_stdsys_optimistic_events(
    this->bridge, praef_simple_impl_optimistic_events);
  this->event_optimism = cb;
}

static unsigned praef_simple_impl_optimistic_events(
  praef_app* bridge, const praef_event* vevt
) {
  praef_simple_context* this;
  const praef_simple_event* evt;

  this = praef_stdsys_userdata(bridge);
  evt = (const praef_simple_event*)vevt;

  if (!this->event_optimism) return 0;
  return (*this->event_optimism)(
    this, evt->header.object, evt->header.instant,
    evt->header.serial_number, evt + 1);
}
