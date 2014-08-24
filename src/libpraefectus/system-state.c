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

#include "messages/PraefMsg.h"
#include "system.h"
#include "-system.h"
#include "-system-state.h"
#include "defs.h"

static void praef_system_state_loopback_unicast(
  praef_message_bus*, const PraefNetworkIdentifierPair_t*,
  const void*, size_t);

static void praef_system_state_loopback_broadcast(
  praef_message_bus*, const void*, size_t);

static void praef_system_state_recv_message(
  praef_system*, praef_hlmsg*);
static void praef_system_state_process_message(
  praef_system*, praef_node*, praef_instant, PraefMsg_t*);

static void praef_system_state_process_appevt(
  praef_system*, praef_node*, praef_instant, PraefMsgAppEvent_t*);
static void praef_system_state_process_vote(
  praef_system*, praef_node*, praef_instant, PraefMsgVote_t*);

int praef_system_state_init(praef_system* sys) {
  sys->state.loopback.unicast = praef_system_state_loopback_unicast;
  sys->state.loopback.broadcast = praef_system_state_loopback_broadcast;

  if (!(sys->state.ur_mq = praef_mq_new(sys->router.ur_out,
                                        &sys->state.loopback,
                                        NULL)))
    return 0;

  return 1;
}

void praef_system_state_destroy(praef_system* sys) {
  praef_mq_delete(sys->state.ur_mq);
}

void praef_system_state_update(praef_system* sys, unsigned delta) {
  unsigned char data[65536];
  size_t size;
  praef_hlmsg msg;

  /* Poll for messages from the main bus */
  while ((size = (*sys->bus->recv)(data, sizeof(data)-1, sys->bus))) {
    data[size] = 0;

    msg.data = data;
    msg.size = size+1;
    praef_system_state_recv_message(sys, &msg);
  }

  /* Pump any locally-produced uncommitted-redistributable messages */
  praef_mq_update(sys->state.ur_mq);
}

int praef_node_state_init(praef_node* node) {
  /* The node's clock source is only meaningful if the node is not the local
   * node.
   */
  if (node->sys->local_node != node)
    praef_clock_source_init(&node->state.clock_source, &node->sys->clock);

  return 1;
}

void praef_node_state_destroy(praef_node* node) {
  if (node->sys->local_node != node)
    praef_clock_source_destroy(&node->state.clock_source, &node->sys->clock);
}

static void praef_system_state_loopback_broadcast(
  praef_message_bus* bus, const void* data, size_t size
) {
  /* We know that the message is coming from a praef_mq, so data can be
   * directly used as the payload of an hlmsg.
   */
  praef_hlmsg msg;

  msg.data = data;
  msg.size = size+1;
  praef_system_state_recv_message(
    UNDOT(praef_system, state,
          UNDOT(praef_system_state, loopback, bus)),
    &msg);
}

static void praef_system_state_loopback_unicast(
  praef_message_bus* bus, const PraefNetworkIdentifierPair_t* ignore,
  const void* data, size_t size
) {
  praef_system_state_loopback_broadcast(bus, data, size);
}

static void praef_system_state_recv_message(
  praef_system* sys, praef_hlmsg* msg
) {
  praef_object_id sender_id;
  praef_node* sender;
  const praef_hlmsg_segment* seg;
  PraefMsg_t* decoded;
  praef_instant instant;

  if (!praef_hlmsg_is_valid(msg)) return;

  /* TODO (probably not exhastive):
   *
   * - Add messages to hash tree when appropriate
   * - Add messages to commitment chains when appropriate
   * - Drop duplicated messages (ie, those already in the hash tree)
   * - Filter messages by time (in some cases)
   * - Sample clock source
   */

  sender_id = praef_verifier_verify(
    sys->verifier, praef_hlmsg_pubkey_hint(msg),
    praef_hlmsg_signature(msg),
    praef_hlmsg_signable(msg), praef_hlmsg_signable_sz(msg));
  if (sender_id)
    sender = RB_FIND(praef_node_map, &sys->nodes, (praef_node*)&sender_id);
  else
    sender = NULL;

  instant = praef_hlmsg_instant(msg);

  for (seg = praef_hlmsg_first(msg); seg; seg = praef_hlmsg_snext(seg)) {
    decoded = praef_hlmsg_sdec(seg);
    if (!decoded) {
      sys->oom = 1;
    } else {
      praef_system_state_process_message(sys, sender, instant, decoded);
      (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, decoded, 0);
    }
  }
}

static void praef_system_state_process_message(
  praef_system* sys, praef_node* sender, praef_instant instant, PraefMsg_t* msg
) {
  switch (msg->present) {
  case PraefMsg_PR_NOTHING: abort();
  case PraefMsg_PR_appevt:
    if (sender)
      praef_system_state_process_appevt(sys, sender, instant, &msg->choice.appevt);
    break;

  case PraefMsg_PR_vote:
    if (sender)
      praef_system_state_process_vote(sys, sender, instant, &msg->choice.vote);
    break;

  /* TODO: Handle all cases, remove default */
  default:
    abort();
  }
}

static void praef_system_state_process_appevt(
  praef_system* sys, praef_node* sender, praef_instant instant,
  PraefMsgAppEvent_t* msg
) {
  /* TODO: Check for duplicate event */
  praef_event* evt = (*sys->app->decode_event)(
    sys->app, instant, sender->id,
    msg->serialnumber,
    msg->data.buf, msg->data.size);
  if (!evt) {
    /* Invalid event */
    sender->disposition = praef_nd_negative;
    return;
  }

  /* TODO: Store the event somewhere for duplicate checking */
  (*sys->app->insert_event_bridge)(sys->app, evt);
}


static void praef_system_state_process_vote(
  praef_system* sys, praef_node* sender, praef_instant instant,
  PraefMsgVote_t* msg
) {
  /* TODO: Enforce time boundaries on msg->instant vs hlmsg instant */
  (*sys->app->vote_bridge)(sys->app, sender->id, msg->instant,
                           msg->serialnumber);
}

void praef_node_state_update(praef_node* node, unsigned delta) {
}
