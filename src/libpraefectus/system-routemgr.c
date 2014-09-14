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
#include "secure-random.h"
#include "messages/PraefMsg.h"

int praef_system_routemgr_init(praef_system* sys) {
  sys->routemgr.ungranted_route_interval = 4 * sys->std_latency;
  sys->routemgr.granted_route_interval = 32 * sys->std_latency;
  sys->routemgr.ping_interval = 16 * sys->std_latency;
  sys->routemgr.max_pong_silence = 128 * sys->std_latency;

  if (!praef_secure_random(sys->routemgr.ping_salt,
                           sizeof(sys->routemgr.ping_salt)))
    return 0;

  return 1;
}

void praef_system_routemgr_destroy(praef_system* sys) { }

int praef_node_routemgr_init(praef_node* node) {
  return 1;
}

void praef_node_routemgr_destroy(praef_node* node) { }

void praef_system_routemgr_recv_msg_route(
  praef_system* sys, const PraefMsgRoute_t* msg
) {
  praef_node* other = praef_system_get_node(sys, msg->node);
  if (other && praef_nd_neutral == other->disposition)
    other->disposition = praef_nd_positive;
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

