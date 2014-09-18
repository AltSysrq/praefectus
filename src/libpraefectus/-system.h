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
#ifndef LIBPRAEFECTUS__SYSTEM_H_
#define LIBPRAEFECTUS__SYSTEM_H_

#include <stddef.h>

/* This is an internal header file.
 *
 * It defines the system struct and internal interface, primarily so that tests
 * can examine things more directly.
 */

#include "system.h"
#include "dsa.h"
#include "clock.h"

/* The praef_system implementation is split into two layers. The routing layer
 * takes inputs from the application and encodes messages, which are sent to
 * other nodes and to the state layer. The state layer manages the system
 * state, generally oblivious to the concept of a local node except for
 * time-keeping purposes.
 *
 * Other "side-car" components handle isolated concerns such as RPC protocols
 * or node joining procedures.
 *
 * This design does incur meaningful computational overhead, since even when
 * the local node is alone, it still goes through the motions of encoding
 * everything, aggregating into packets, signing packets, verifying packets,
 * and validating the commit chain for itself. However, this makes most of the
 * internal process a lot simpler, and ensures that the application's
 * encoding/decoding is exercised even in local testing.
 */

#include "-system-state.h"
#include "-system-router.h"
#include "-system-join.h"
#include "-system-htm.h"
#include "-system-routemgr.h"
#include "-system-mod.h"
#include "-system-commit.h"

typedef enum {
  praef_nd_neutral = 0,
  praef_nd_positive,
  praef_nd_negative
} praef_node_disposition;

typedef enum {
  praef_sjs_unconnected = 0,
  praef_sjs_request_cxn,
  praef_sjs_walking_join_tree,
  praef_sjs_scanning_hash_tree,
  praef_sjs_requesting_grant,
  praef_sjs_connected
} praef_system_join_state;

typedef struct praef_node_s {
  praef_object_id id;
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];
  PraefNetworkIdentifierPair_t net_id;
  praef_system* sys;
  praef_message_bus* bus;
  praef_instant created_at;

  praef_node_disposition disposition;

  praef_node_state state;
  praef_node_router router;
  praef_node_join join;
  praef_node_htm htm;
  praef_node_routemgr routemgr;
  praef_node_mod mod;
  praef_node_commit commit;

  RB_ENTRY(praef_node_s) map;
} praef_node;

RB_HEAD(praef_node_map, praef_node_s);

struct praef_system_s {
  praef_app* app;
  praef_message_bus* bus;
  unsigned std_latency;
  praef_system_profile profile;
  praef_system_ip_version ip_version;
  praef_system_network_locality net_locality;
  unsigned mtu;
  unsigned grace_period;
  const PraefNetworkIdentifierPair_t* self_net_id;

  praef_signator* signator;
  praef_verifier* verifier;
  praef_event_serial_number evt_serno;
  praef_clock clock;
  praef_system_join_state join_state;

  praef_system_state state;
  praef_system_router router;
  praef_system_join join;
  praef_system_htm htm;
  praef_system_routemgr routemgr;
  praef_system_mod mod;
  praef_system_commit commit;

  struct praef_node_map nodes;

  praef_node* local_node;
  praef_system_status abnormal_status;
};

praef_node* praef_system_get_node(praef_system*, praef_object_id);
praef_node* praef_node_new(
  praef_system*, praef_object_id,
  const PraefNetworkIdentifierPair_t*,
  praef_message_bus*, praef_node_disposition,
  const unsigned char pubkey[PRAEF_PUBKEY_SIZE]);
void praef_node_delete(praef_node*);
/**
 * Registers the given node. Fails if the node is already registered or the
 * public key is duplicated. In such a case, the node is destroyed.
 *
 * @return Whether the node was registered.
 */
int praef_system_register_node(praef_system*, praef_node*);

int praef_system_is_permissible_netid(
  praef_system*, const PraefNetworkIdentifierPair_t*);
int praef_system_net_id_pair_equal(
  const PraefNetworkIdentifierPair_t*, const PraefNetworkIdentifierPair_t*);

int praef_node_is_in_grace_period(praef_node*);
int praef_node_has_grant(praef_node*);
int praef_node_has_deny(praef_node*);
int praef_node_is_alive(praef_node*);

int praef_compare_nodes(const praef_node*, const praef_node*);
RB_PROTOTYPE(praef_node_map, praef_node_s, map, praef_compare_nodes)

#define PRAEF_OOM_IF_NOT(sys, cond) do {                \
    if (!(cond)) (sys)->abnormal_status = praef_ss_oom; \
  } while (0)

#define PRAEF_OOM_IF(sys, cond) do {                    \
    if (cond) (sys)->abnormal_status = praef_ss_oom;    \
  } while (0)

static inline void praef_system_oom_if_not(
  praef_system* sys, int cond
) {
  if (!cond) sys->abnormal_status = praef_ss_oom;
}

#define PRAEF_APP_HAS(app, method)                      \
  (((app)->size >= offsetof(praef_app, method)) &&      \
   (app)->method)

#endif /* LIBPRAEFECTUS__SYSTEM_H_ */
