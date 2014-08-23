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

#include <string.h>

#include "system.h"
#include "-system.h"

int praef_compare_nodes(const praef_node* a, const praef_node* b) {
  return (a->id > b->id) - (a->id < b->id);
}

RB_GENERATE(praef_node_map, praef_node_s, map, praef_compare_nodes)

praef_system* praef_system_new(praef_app* app,
                               praef_message_bus* bus,
                               const PraefNetworkIdentifierPair_t* self,
                               unsigned std_latency,
                               praef_system_profile profile,
                               unsigned mtu) {
  praef_system* this;

  this = calloc(1, sizeof(praef_system));
  if (!this) return NULL;

  this->app = app;
  this->bus = bus;
  this->mtu = mtu;
  this->std_latency = std_latency;
  this->profile = profile;
  praef_clock_init(&this->clock, 5 * std_latency, std_latency);
  RB_INIT(&this->nodes);

  if (!(this->signator = praef_signator_new()) ||
      !(this->verifier = praef_verifier_new()) ||
      !praef_system_router_init(this) ||
      !praef_system_state_init(this)) {
    praef_system_delete(this);
    return NULL;
  }

  return this;
}

void praef_system_delete(praef_system* this) {
  praef_node* node, * tmp;

  for (node = RB_MIN(praef_node_map, &this->nodes);
       node; node = tmp) {
    tmp = RB_NEXT(praef_node_map, &this->nodes, node);
    RB_REMOVE(praef_node_map, &this->nodes, node);

    praef_node_state_destroy(node);
    praef_node_router_destroy(node);
    free(node);
  }

  praef_system_state_destroy(this);
  praef_system_router_destroy(this);

  if (this->verifier) praef_verifier_delete(this->verifier);
  if (this->signator) praef_signator_delete(this->signator);
  free(this);
}

int praef_system_add_event(praef_system* this, const void* data, size_t size) {
  PraefMsg_t msg;
  int ok = 1;

  memset(&msg, 0, sizeof(msg));
  msg.present = PraefMsg_PR_appevt;
  msg.choice.appevt.serialnumber = this->evt_serno++;
  ok &= !OCTET_STRING_fromBuf(&msg.choice.appevt.data, data, size);

  if (ok) ok &= praef_outbox_append(this->router.cr_out, &msg);

  (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, &msg, 1);
  return ok;
}

int praef_system_vote_event(praef_system* this, praef_object_id oid,
                            praef_instant instant,
                            praef_event_serial_number serno) {
  PraefMsg_t msg;

  memset(&msg, 0, sizeof(msg));
  msg.present = PraefMsg_PR_vote;
  msg.choice.vote.node = oid;
  msg.choice.vote.instant = instant;
  msg.choice.vote.serialnumber = serno;

  return praef_outbox_append(this->router.cr_out, &msg);
}

static void praef_system_register_node(
  praef_system* this, praef_node* node,
  const unsigned char pubkey[PRAEF_PUBKEY_SIZE]
) {
  /* TODO: Properly handle duplicate public key */
  this->oom |= !praef_verifier_assoc(this->verifier, pubkey, node->id);
  RB_INSERT(praef_node_map, &this->nodes, node);
  (*this->app->create_node_object)(this->app, node->id);
}

void praef_system_bootstrap(praef_system* this) {
  praef_node* node = calloc(1, sizeof(praef_node));
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];
  if (!node) {
    this->oom = 1;
    return;
  }

  node->id = 1;
  node->sys = this;
  node->bus = &this->state.loopback;
  node->disposition = praef_nd_positive;
  this->local_node = node;

  praef_signator_pubkey(pubkey, this->signator);
  praef_system_register_node(this, node, pubkey);

  this->oom |=
    !praef_node_router_init(node) ||
    !praef_node_state_init(node);

  if (PRAEF_APP_HAS(this->app, acquire_id_opt))
    (*this->app->acquire_id_opt)(this->app, 1);
}

static int praef_system_self_alive(praef_system* this) {
  if (!this->local_node) return 0;

  return
    (this->clock.monotime <
     (*this->app->get_node_grant_bridge)(this->app, this->local_node->id)) &&
    (this->clock.monotime >=
     (*this->app->get_node_deny_bridge)(this->app, this->local_node->id));
}

praef_system_status praef_system_advance(praef_system* this, unsigned elapsed) {
  unsigned old_monotime = this->clock.monotime, elapsed_monotime;
  praef_node* node;
  praef_clock_tick(&this->clock, elapsed, praef_system_self_alive(this));
  elapsed_monotime = this->clock.monotime - old_monotime;

  RB_FOREACH(node, praef_node_map, &this->nodes) {
    praef_node_state_update(node, elapsed);
    praef_node_router_update(node, elapsed);
  }

  praef_system_state_update(this, elapsed);
  (*this->app->advance_bridge)(this->app, elapsed_monotime);
  praef_system_router_update(this, elapsed);

  if (this->oom) return praef_ss_oom;
  /* TODO: Other stati */
  return praef_ss_ok;
}
