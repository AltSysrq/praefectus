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

#include "-system.h"
#include "keccak.h"
#include "secure-random.h"
#include "messages/PraefMsg.h"

static void praef_node_routemgr_send_routes(praef_node*);
static void praef_node_routemgr_send_ping(praef_node*);

void praef_system_conf_ungranted_route_interval(praef_system* sys, unsigned i) {
  sys->routemgr.ungranted_route_interval = i;
}

void praef_system_conf_granted_route_interval(praef_system* sys, unsigned i) {
  sys->routemgr.granted_route_interval = i;
}

void praef_system_conf_ping_interval(praef_system* sys, unsigned i) {
  sys->routemgr.ping_interval = i;
}

void praef_system_conf_max_pong_silence(praef_system* sys, unsigned i) {
  sys->routemgr.max_pong_silence = i;
}

void praef_system_conf_route_kill_delay(praef_system* sys, unsigned i) {
  sys->routemgr.route_kill_delay = i;
}

int praef_system_routemgr_init(praef_system* sys) {
  sys->routemgr.ungranted_route_interval = 4 * sys->std_latency;
  sys->routemgr.granted_route_interval = 32 * sys->std_latency;
  sys->routemgr.ping_interval = 16 * sys->std_latency;
  sys->routemgr.max_pong_silence = 128 * sys->std_latency;
  sys->routemgr.route_kill_delay = 256 * sys->std_latency;

  if (!praef_secure_random(sys->routemgr.ping_salt,
                           sizeof(sys->routemgr.ping_salt)))
    return 0;

  return 1;
}

void praef_system_routemgr_destroy(praef_system* sys) { }

int praef_node_routemgr_init(praef_node* node) {
  node->routemgr.last_pong = node->sys->clock.ticks;
  return 1;
}

void praef_node_routemgr_destroy(praef_node* node) { }

void praef_system_routemgr_recv_msg_route(
  praef_system* sys, const PraefMsgRoute_t* msg
) {
  praef_node* other = praef_system_get_node(sys, msg->node);

  if (other && praef_nd_neutral == other->disposition)
    other->disposition = praef_nd_positive;

  /* TODO: We should do something with the attached latency here */
}

void praef_node_routemgr_recv_msg_ping(
  praef_node* from, const PraefMsgPing_t* msg
) {
  PraefMsg_t response;

  memset(&response, 0, sizeof(response));
  response.present = PraefMsg_PR_pong;
  response.choice.pong.id = msg->id;
  PRAEF_OOM_IF_NOT(from->sys, praef_outbox_append(
                     from->router.rpc_out, &response));
}

void praef_node_routemgr_recv_msg_pong(
  praef_node* node, const PraefMsgPong_t* msg
) {
  unsigned i, sum;

  if (msg->id != node->routemgr.current_ping_id ||
      !node->routemgr.in_flight_ping)
    /* Uncorrelated or unsolicited response, discard */
    return;

  node->routemgr.in_flight_ping = 0;
  node->routemgr.last_pong = node->sys->clock.ticks;
  memmove(node->routemgr.latency_samples+1,
          node->routemgr.latency_samples,
          sizeof(node->routemgr.latency_samples) - sizeof(unsigned));
  node->routemgr.latency_samples[0] =
    node->routemgr.last_pong - node->routemgr.last_ping;

  sum = 0;
  for (i = 0; i < PRAEF_NODE_ROUTEMGR_NUM_LATENCY_SAMPLES; ++i)
    sum += node->routemgr.latency_samples[i];

  node->routemgr.latency = sum / PRAEF_NODE_ROUTEMGR_NUM_LATENCY_SAMPLES;
}

static int praef_node_routemgr_should_kill(praef_node* node) {
  praef_instant deny = (*node->sys->app->get_node_deny_bridge)(
    node->sys->app, node->id);

  /* Seemingly redundant check to avoid overflow */
  return deny < node->sys->clock.monotime &&
    deny + node->sys->routemgr.route_kill_delay <
    node->sys->clock.monotime;
}

void praef_node_routemgr_update(praef_node* node) {
  unsigned route_interval;

  /* Ignoring the local node, we want to ensure there is a route to any
   * positive node, and that there is no route to any negative node which has
   * had DENY for the requisite amount of time.
   */
  if (node != node->sys->local_node) {
    if (praef_nd_positive == node->disposition && !node->router.cr_mq) {
      praef_system_log(node->sys, "Creating route to %08X", node->id);
      (*node->bus->create_route)(node->bus, &node->net_id);
      node->router.cr_mq = praef_mq_new(node->sys->router.cr_out,
                                        node->bus,
                                        &node->net_id);
      if (node->router.cr_mq)
        praef_mq_set_threshold(node->router.cr_mq,
                               praef_node_visibility_threshold(node));
      else
        praef_system_oom(node->sys);
    } else if (praef_nd_negative == node->disposition &&
               node->router.cr_mq &&
               praef_node_routemgr_should_kill(node)) {
      praef_system_log(node->sys, "Destroying route to %08X", node->id);
      (*node->bus->delete_route)(node->bus, &node->net_id);
      praef_mq_delete(node->router.cr_mq);
      node->router.cr_mq = NULL;
    }
  }

  if (praef_node_has_deny(node) || praef_nd_negative == node->disposition)
    return;

  if (node->sys->clock.ticks - node->routemgr.last_pong >
      node->sys->routemgr.max_pong_silence) {
    praef_node_negative(node, "Pong silence exceeded; "
                        "last pong = %d ticks, now = %d ticks",
                        node->routemgr.last_pong,
                        node->sys->clock.ticks);
    return;
  }

  route_interval = praef_node_has_grant(node)?
    node->sys->routemgr.granted_route_interval :
    node->sys->routemgr.ungranted_route_interval;

  if (node->sys->clock.ticks - node->routemgr.last_route_message >=
      route_interval)
    praef_node_routemgr_send_routes(node);

  if (node->sys->clock.ticks - node->routemgr.last_ping >=
      node->sys->routemgr.ping_interval)
    praef_node_routemgr_send_ping(node);
}

static void praef_node_routemgr_send_routes(praef_node* to) {
  PraefMsg_t msg;
  praef_node* node;

  memset(&msg, 0, sizeof(msg));
  msg.present = PraefMsg_PR_route;

  RB_FOREACH(node, praef_node_map, &to->sys->nodes) {
    if (praef_nd_positive == node->disposition) {
      msg.choice.route.node = node->id;
      if (node->routemgr.latency <= 255)
        msg.choice.route.latency = node->routemgr.latency;
      else
        msg.choice.route.latency = 255;

      PRAEF_OOM_IF_NOT(to->sys, praef_outbox_append(
                         to->router.rpc_out, &msg));
    }
  }

  to->routemgr.last_route_message = to->sys->clock.ticks;
}

static void praef_node_routemgr_send_ping(praef_node* to) {
  praef_keccak_sponge sponge;
  PraefMsg_t msg;

  memset(&msg, 0, sizeof(msg));
  msg.present = PraefMsg_PR_ping;
  praef_sha3_init(&sponge);
  praef_keccak_sponge_absorb(&sponge, to->sys->routemgr.ping_salt,
                             sizeof(to->sys->routemgr.ping_salt));
  /* Since this is just an RNG, don't care about endianness. */
  praef_keccak_sponge_absorb(&sponge, (void*)&to->routemgr.current_ping_id,
                             sizeof(to->routemgr.current_ping_id));
  praef_keccak_sponge_squeeze(&sponge, (void*)&to->routemgr.current_ping_id,
                              sizeof(to->routemgr.current_ping_id));
  praef_keccak_sponge_squeeze(&sponge, to->sys->routemgr.ping_salt,
                              sizeof(to->sys->routemgr.ping_salt));
  msg.choice.ping.id = to->routemgr.current_ping_id;
  PRAEF_OOM_IF_NOT(to->sys, praef_outbox_append(
                     to->router.rpc_out, &msg));

  to->routemgr.in_flight_ping = 1;
  to->routemgr.last_ping = to->sys->clock.ticks;
}
