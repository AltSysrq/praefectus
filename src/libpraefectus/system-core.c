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

#include <stdlib.h>

#include "common.h"
#include "system.h"
#include "-system.h"

static int praef_clone_net_id(PraefNetworkIdentifierPair_t*,
                              const PraefNetworkIdentifierPair_t*);
static int praef_clone_snet_id(PraefNetworkIdentifier_t*,
                               const PraefNetworkIdentifier_t*);

RB_GENERATE(praef_extnode_map, praef_extnode_s, map, praef_compare_extnode)

int praef_compare_extnode(const praef_extnode* pa, const praef_extnode* pb) {
  praef_object_id a = pa->id, b = pb->id;

  return (a > b) - (a < b);
}

praef_system* praef_system_new(praef_app* app,
                               praef_message_bus* bus,
                               unsigned std_latency,
                               praef_system_profile profile,
                               const PraefNetworkIdentifierPair_t* net_id) {
  praef_system* this = calloc(1, sizeof(praef_system));
  if (!this) return NULL;

  this->app = app;
  this->bus = bus;
  RB_INIT(&this->nodes);
  praef_clock_init(&this->clock, std_latency*5, std_latency);

  if (!praef_clone_net_id(&this->net_id, net_id)) goto fail;
  if (!(this->hash_tree = praef_hash_tree_new())) goto fail;
  if (!(this->signator = praef_signator_new())) goto fail;
  if (!(this->verifier = praef_verifier_new())) goto fail;

  return this;

  fail:
  praef_system_delete(this);
  return NULL;
}

void praef_system_delete(praef_system* this) {
  praef_extnode* node, * tmp;

  for (node = RB_MIN(praef_extnode_map, &this->nodes);
       node; node = tmp) {
    tmp = RB_NEXT(praef_extnode_map, &this->nodes, node);
    RB_REMOVE(praef_extnode_map, &this->nodes, node);
    praef_extnode_delete(node);
  }

  (*asn_DEF_PraefNetworkIdentifierPair.free_struct)(
    &asn_DEF_PraefNetworkIdentifierPair, &this->net_id, 1);

  if (this->verifier) praef_verifier_delete(this->verifier);
  if (this->signator) praef_signator_delete(this->signator);
  if (this->hash_tree) praef_hash_tree_delete(this->hash_tree);
  free(this);
}

praef_extnode* praef_extnode_new(praef_system* system,
                                 praef_object_id id,
                                 const PraefNetworkIdentifierPair_t* net_id) {
  praef_extnode* this = calloc(1, sizeof(praef_extnode));
  if (!this) return NULL;

  this->id = id;
  this->system = system;
  praef_clock_source_init(&this->clock_source, &system->clock);

  if (!praef_clone_net_id(&this->net_id, net_id)) goto fail;
  if (!(this->comchain = praef_comchain_new())) goto fail;

  return this;

  fail:
  praef_extnode_delete(this);
  return NULL;
}


void praef_extnode_delete(praef_extnode* this) {
  (*asn_DEF_PraefNetworkIdentifierPair.free_struct)(
    &asn_DEF_PraefNetworkIdentifierPair, &this->net_id, 1);

  if (this->comchain) praef_comchain_delete(this->comchain);

  praef_clock_source_destroy(&this->clock_source, &this->system->clock);

  free(this);
}

static int praef_clone_net_id(
  PraefNetworkIdentifierPair_t* dst,
  const PraefNetworkIdentifierPair_t* src
) {
  if (src->internet) {
    if (!(dst->internet = calloc(1, sizeof(PraefNetworkIdentifier_t))))
      return 0;

    if (!praef_clone_snet_id(dst->internet, src->internet))
      return 0;
  } else {
    dst->internet = NULL;
  }

  return praef_clone_snet_id(&dst->intranet, &src->intranet);
}

static int praef_clone_snet_id(PraefNetworkIdentifier_t* dst,
                               const PraefNetworkIdentifier_t* src) {
  dst->port = src->port;
  dst->address.present = src->address.present;
  switch (dst->address.present) {
  case PraefIpAddress_PR_ipv4:
    if (OCTET_STRING_fromBuf(&dst->address.choice.ipv4,
                             (const char*)src->address.choice.ipv4.buf,
                             src->address.choice.ipv4.size))
      return 0;
    break;

  case PraefIpAddress_PR_ipv6:
    if (OCTET_STRING_fromBuf(&dst->address.choice.ipv6,
                             (const char*)src->address.choice.ipv6.buf,
                             src->address.choice.ipv6.size))
      return 0;
    break;

  case PraefIpAddress_PR_NOTHING:
    break;
  }

  return 1;
}

void praef_system_conf_clock_obsolescence_interval(praef_system* this,
                                                   unsigned ival) {
  this->clock.obsolescence_interval = ival;
}

void praef_system_conf_clock_tolerance(praef_system* this,
                                       unsigned tol) {
  this->clock.tolerance = tol;
}
