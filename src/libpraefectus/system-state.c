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
#include "-system-join.h"
#include "defs.h"

typedef struct {
  praef_event self;
  praef_event* actual;
} praef_system_state_wrapped_event;

static void praef_system_state_loopback_unicast(
  praef_message_bus*, const PraefNetworkIdentifierPair_t*,
  const void*, size_t);

static void praef_system_state_loopback_broadcast(
  praef_message_bus*, const void*, size_t);

static void praef_system_state_process_message(
  praef_system*, praef_node*, praef_instant,
  PraefMsg_t*, const praef_hlmsg*);

static void praef_system_state_process_appevt(
  praef_system*, praef_node*, praef_instant, PraefMsgAppEvent_t*);
static void praef_system_state_process_vote(
  praef_system*, praef_node*, praef_instant, PraefMsgVote_t*);

void praef_system_conf_max_event_vote_offset(praef_system* sys, unsigned i) {
  sys->state.max_event_vote_offset = i;
}

int praef_system_state_init(praef_system* sys) {
  sys->state.loopback.unicast = praef_system_state_loopback_unicast;
  sys->state.loopback.broadcast = praef_system_state_loopback_broadcast;
  sys->state.max_event_vote_offset = ~0u;

  SPLAY_INIT(&sys->state.present_events);
  SPLAY_INIT(&sys->state.present_votes);

  if (!(sys->state.ur_mq = praef_mq_new(sys->router.ur_out,
                                        &sys->state.loopback,
                                        NULL)) ||
      !(sys->state.hash_tree = praef_hash_tree_new()))
    return 0;

  return 1;
}

void praef_system_state_destroy(praef_system* sys) {
  praef_event* pe, * pe_tmp;
  praef_system_state_present_vote* pv, * pv_tmp;

  if (sys->state.ur_mq) praef_mq_delete(sys->state.ur_mq);
  if (sys->state.hash_tree) praef_hash_tree_delete(sys->state.hash_tree);

  for (pe = SPLAY_MIN(praef_event_sequence, &sys->state.present_events);
       pe; pe = pe_tmp) {
    pe_tmp = SPLAY_NEXT(praef_event_sequence, &sys->state.present_events, pe);
    SPLAY_REMOVE(praef_event_sequence, &sys->state.present_events, pe);
    free(pe);
  }

  for (pv = SPLAY_MIN(praef_system_state_present_votes,
                      &sys->state.present_votes);
       pv; pv = pv_tmp) {
    pv_tmp = SPLAY_NEXT(praef_system_state_present_votes,
                        &sys->state.present_votes, pv);
    SPLAY_REMOVE(praef_system_state_present_votes,
                 &sys->state.present_votes, pv);
    free(pv);
  }
}

void praef_system_state_update(praef_system* sys) {
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

void praef_system_state_recv_message(
  praef_system* sys, praef_hlmsg* msg
) {
  praef_object_id sender_id;
  praef_node* sender;
  const praef_hlmsg_segment* seg;
  PraefMsg_t* decoded;
  praef_instant instant;
  praef_hash_tree_objref ht_objref;
  praef_hlmsg msg_in_ht;

  if (!praef_hlmsg_is_valid(msg)) return;
  instant = praef_hlmsg_instant(msg);

  sender_id = praef_verifier_verify(
    sys->verifier, praef_hlmsg_pubkey_hint(msg),
    praef_hlmsg_signature(msg),
    praef_hlmsg_signable(msg), praef_hlmsg_signable_sz(msg));
  if (sender_id)
    sender = RB_FIND(praef_node_map, &sys->nodes, (praef_node*)&sender_id);
  else
    sender = NULL;

  if (sender &&
      sender != sys->local_node &&
      praef_node_is_alive(sender))
    praef_clock_source_sample(&sender->state.clock_source, &sys->clock,
                              instant, sender->routemgr.latency);

  if (sender && praef_htf_rpc_type != praef_hlmsg_type(msg)) {
    ht_objref.size = msg->size - 1;
    ht_objref.data = msg->data;
    switch (praef_hash_tree_add(sys->state.hash_tree, &ht_objref)) {
    case praef_htar_failed:
      sys->abnormal_status = praef_ss_oom;
      return;

    case praef_htar_already_present:
      /* Duplicate message, no more processing needed */
      return;

    case praef_htar_added:
      /* Continue processing as normal */
      break;
    }

    /* Neither msg nor the data it points to will be valid after this call
     * returns. The ack table will shallow-copy the hlmsg into itself, so we
     * can use a local variable anyway, but need to point at the data stored in
     * the hash tree, which is actually preserved. (We only reach this point if
     * a new message was added to the hash tree, in which case ht_objref has
     * been populated with data from the hash tree.)
     */
    msg_in_ht.size = msg->size;
    msg_in_ht.data = ht_objref.data;
    praef_node_ack_observe_msg(sender, &msg_in_ht);
  }

  if (praef_htf_committed_redistributable == praef_hlmsg_type(msg) && sender)
    praef_node_commit_observe_message(sender, instant,
                                      praef_hash_tree_get_hash_of(&ht_objref));

  for (seg = praef_hlmsg_first(msg); seg; seg = praef_hlmsg_snext(seg)) {
    decoded = praef_hlmsg_sdec(seg);
    if (!decoded) {
      sys->abnormal_status = praef_ss_oom;
    } else {
      praef_system_state_process_message(sys, sender, instant, decoded, msg);
      (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, decoded, 0);
    }
  }
}

static void praef_system_state_process_message(
  praef_system* sys, praef_node* sender, praef_instant instant,
  PraefMsg_t* msg, const praef_hlmsg* envelope
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

  case PraefMsg_PR_chmod:
    if (sender)
      praef_node_mod_recv_msg_chmod(sender, instant, &msg->choice.chmod);
    break;

  case PraefMsg_PR_getnetinfo:
    praef_system_join_recv_msg_get_network_info(sys, &msg->choice.getnetinfo);
    break;

  case PraefMsg_PR_netinfo:
    praef_system_join_recv_msg_network_info(sys, &msg->choice.netinfo);
    break;

  case PraefMsg_PR_joinreq:
    praef_system_join_recv_msg_join_request(sys, sender, &msg->choice.joinreq,
                                            envelope);
    break;

  case PraefMsg_PR_accept:
    praef_system_join_recv_msg_join_accept(sys, sender, &msg->choice.accept,
                                           envelope);
    break;

  case PraefMsg_PR_jointree:
    if (sender)
      praef_system_join_recv_msg_join_tree(sender, &msg->choice.jointree);
    break;

  case PraefMsg_PR_jtentry:
    praef_system_join_recv_msg_join_tree_entry(sys, &msg->choice.jtentry);
    break;

  case PraefMsg_PR_htls:
    if (sender)
      praef_node_htm_recv_msg_htls(sender, &msg->choice.htls);
    break;

  case PraefMsg_PR_htdir:
    if (sender)
      praef_node_htm_recv_msg_htdir(sender, &msg->choice.htdir);
    break;

  case PraefMsg_PR_htread:
    if (sender)
      praef_node_htm_recv_msg_htread(sender, &msg->choice.htread);
    break;

  case PraefMsg_PR_htrange:
    if (sender)
      praef_node_htm_recv_msg_htrange(sender, &msg->choice.htrange);
    break;

  case PraefMsg_PR_htrangenext:
    if (sender)
      praef_node_htm_recv_msg_htrangenext(sender, &msg->choice.htrangenext);
    break;

  case PraefMsg_PR_ping:
    if (sender)
      praef_node_routemgr_recv_msg_ping(sender, &msg->choice.ping);
    break;

  case PraefMsg_PR_pong:
    if (sender)
      praef_node_routemgr_recv_msg_pong(sender, &msg->choice.pong);
    break;

  case PraefMsg_PR_route:
    if (sender)
      praef_system_routemgr_recv_msg_route(sys, &msg->choice.route);
    break;

  case PraefMsg_PR_commit:
    if (sender)
      praef_node_commit_recv_msg_commit(sender, instant, &msg->choice.commit);
    break;

  case PraefMsg_PR_received:
    if (sender)
      praef_node_ack_recv_msg_received(sender, &msg->choice.received);
    break;

  case PraefMsg_PR_appuni:
    if (sender && praef_node_is_alive(sender) &&
        PRAEF_APP_HAS(sys->app, recv_unicast_opt))
      (*sys->app->recv_unicast_opt)(
        sys->app, sender->id, instant,
        msg->choice.appuni.data.buf,
        msg->choice.appuni.data.size);
    break;
  }
}

static void praef_system_state_process_appevt(
  praef_system* sys, praef_node* sender, praef_instant instant,
  PraefMsgAppEvent_t* msg
) {
  praef_system_state_wrapped_event* wrapped;
  praef_event* already_existing;
  praef_event* evt = (*sys->app->decode_event)(
    sys->app, instant, sender->id,
    msg->serialnumber,
    msg->data.buf, msg->data.size);
  if (!evt) {
    /* Invalid event */
    sender->disposition = praef_nd_negative;
    return;
  }

  if ((already_existing =
       SPLAY_FIND(praef_event_sequence, &sys->state.present_events, evt))) {
    /* An event with this identifying triple already exists. Neutralise the
     * original and do not process this one.
     */
    wrapped = (praef_system_state_wrapped_event*)already_existing;
    (*sys->app->neutralise_event_bridge)(sys->app, wrapped->actual);
    (*evt->free)(evt);
    sender->disposition = praef_nd_negative;
  }

  (*sys->app->insert_event_bridge)(sys->app, evt);

  /* Record this event triple so that we can't produce a duplicate event
   * later.
   */
  wrapped = malloc(sizeof(praef_system_state_wrapped_event));
  if (!wrapped) {
    praef_system_oom(sys);
    return;
  }

  memcpy(wrapped, evt, sizeof(praef_event));
  wrapped->self.apply = NULL;
  wrapped->self.free = NULL;
  wrapped->actual = evt;
  SPLAY_INSERT(praef_event_sequence, &sys->state.present_events,
               (praef_event*)wrapped);
}


static void praef_system_state_process_vote(
  praef_system* sys, praef_node* sender, praef_instant instant,
  PraefMsgVote_t* msg
) {
  praef_system_state_present_vote example, * entry;

  if ((msg->instant < instant &&
       instant - msg->instant > sys->state.max_event_vote_offset) ||
      (instant < msg->instant &&
       msg->instant - instant > sys->state.max_event_vote_offset)) {
    sender->disposition = praef_nd_negative;
    return;
  }

  /* Ensure that no such vote has already been sent down */
  example.from_node = sender->id;
  example.against_object = msg->node;
  example.instant = msg->instant;
  example.serial_number = msg->serialnumber;
  if (SPLAY_FIND(praef_system_state_present_votes,
                 &sys->state.present_votes, &example)) {
    sender->disposition = praef_nd_negative;
    return;
  }

  (*sys->app->vote_bridge)(sys->app, sender->id,
                           msg->node,
                           msg->instant,
                           msg->serialnumber);

  /* Record the vote here so we can detect duplicates later */
  entry = malloc(sizeof(praef_system_state_present_vote));
  if (!entry) {
    praef_system_oom(sys);
    return;
  }

  memcpy(entry, &example, sizeof(praef_system_state_present_vote));
  SPLAY_INSERT(praef_system_state_present_votes,
               &sys->state.present_votes, entry);
}

void praef_node_state_update(praef_node* node) { }

int praef_compare_system_state_present_vote(
  const praef_system_state_present_vote* a,
  const praef_system_state_present_vote* b
) {
#define C(f) if (a->f != b->f) return (a->f < b->f) - (b->f < a->f)
  C(instant);
  C(serial_number);
  C(against_object);
  C(from_node);
  return 0;
#undef C
}

SPLAY_GENERATE(praef_system_state_present_votes,
               praef_system_state_present_vote_s,
               tree, praef_compare_system_state_present_vote)
