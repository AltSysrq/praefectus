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
#include <stdlib.h>
#include <assert.h>

#include "messages/PraefMsg.h"
#include "keccak.h"
#include "system.h"
#include "-system.h"
#include "secure-random.h"

static void praef_system_join_query_next_join_tree(praef_system*, praef_node*);
static int praef_system_join_verify_jr_signature(
  praef_system*, const PraefMsgJoinRequest_t*,
  const unsigned char sig[PRAEF_SIGNATURE_SIZE],
  praef_instant);
static unsigned praef_system_join_num_live_nodes(praef_system*);
static void praef_system_join_record_in_join_tree(
  praef_node* from, praef_node* new, const praef_hlmsg*);

int praef_system_join_init(praef_system* sys) {
  sys->join.join_tree_query_interval = sys->std_latency;
  sys->join.accept_interval = sys->std_latency*8;
  sys->join.max_live_nodes = ~0u;

  /* Set up the salt initially with the assumption that the local node will be
   * the bootstrap node. If this winds up not being the case, the salt info
   * from the NetworkInfo message will replace this data.
   */
  if (!praef_secure_random(sys->join.system_salt,
                           sizeof(sys->join.system_salt)))
    return 0;

  praef_signator_sign(sys->join.system_salt_sig,
                      sys->signator,
                      sys->join.system_salt,
                      sizeof(sys->join.system_salt));

  if (!(sys->join.minimal_rpc_encoder =
        praef_hlmsg_encoder_new(praef_htf_rpc_type,
                                sys->signator,
                                &sys->join.minimal_rpc_serno,
                                PRAEF_HLMSG_MTU_MIN, 0)))
    return 0;

  return 1;
}

void praef_system_join_destroy(praef_system* sys) {
  praef_hlmsg_encoder_delete(sys->join.minimal_rpc_encoder);
  if (sys->join.connect_mq)
    praef_mq_delete(sys->join.connect_mq);
  if (sys->join.connect_out)
    praef_outbox_delete(sys->join.connect_out);
}

int praef_node_join_init(praef_node* node) {
  STAILQ_INIT(&node->join.join_tree.children);
  return 1;
}

void praef_node_join_destroy(praef_node* node) { }

void praef_system_conf_join_tree_query_interval(
  praef_system* sys, unsigned interval
) {
  sys->join.join_tree_query_interval = interval;
}

void praef_system_conf_accept_interval(
  praef_system* sys, unsigned interval
) {
  sys->join.accept_interval = interval;
}

void praef_system_conf_max_live_nodes(
  praef_system* sys, unsigned max
) {
  sys->join.max_live_nodes = max;
}

void praef_system_join_update(praef_system* sys) {
  PraefMsg_t request;
  OCTET_STRING_t auth;
  unsigned char auth_data[58];
  unsigned char local_pubkey[PRAEF_PUBKEY_SIZE];
  int has_pending_join_tree_queries;
  praef_node* node;

  /* Nothing else to do here if not currently establishing a connection. */
  if (!sys->join.connect_out) return;

  memset(&request, 0, sizeof(request));
  PRAEF_OOM_IF_NOT(sys, praef_outbox_flush(sys->join.connect_out));
  praef_outbox_set_now(sys->join.connect_out, sys->clock.monotime);

  /* If still in the handshake portion of the connection protocol, retransmit
   * the request packet every frame.
   *
   * Strictly speaking, this is a lot more than necessary, but the requests are
   * idempotent, and 60 packets/sec is a drop in the bucket for any modern
   * network, and is even easy on dial-up.
   */
  if (!sys->join.has_received_network_info) {
    assert(praef_sjs_request_cxn == sys->join_state);

    request.present = PraefMsg_PR_getnetinfo;
    memcpy(&request.choice.getnetinfo.retaddr,
           sys->self_net_id, sizeof(PraefNetworkIdentifierPair_t));
    PRAEF_OOM_IF_NOT(sys, praef_outbox_append_singleton(
                       sys->join.connect_out, &request));
  } else if (!sys->local_node) {
    assert(praef_sjs_request_cxn == sys->join_state);

    praef_signator_pubkey(local_pubkey, sys->signator);
    memset(&auth, 0, sizeof(auth));
    auth.buf = auth_data;
    auth.size = sizeof(auth_data);
    request.present = PraefMsg_PR_joinreq;
    request.choice.joinreq.publickey.buf = local_pubkey;
    request.choice.joinreq.publickey.size = PRAEF_PUBKEY_SIZE;
    memcpy(&request.choice.joinreq.identifier, sys->self_net_id,
           sizeof(PraefNetworkIdentifierPair_t));
    if (PRAEF_APP_HAS(sys->app, gen_auth_opt))
      (*sys->app->gen_auth_opt)(sys->app, &request.choice.joinreq, &auth);
    sys->join.minimal_rpc_serno = 0;
    PRAEF_OOM_IF_NOT(sys, praef_outbox_append_singleton(
                       sys->join.connect_out, &request));
  } else {
    assert(praef_sjs_walking_join_tree == sys->join_state);

    /* See if there are any pending join tree queries against any node. */
    has_pending_join_tree_queries = 0;
    RB_FOREACH(node, praef_node_map, &sys->nodes) {
      if (~0u != node->join.curr_join_tree_query) {
        has_pending_join_tree_queries = 1;
        break;
      }
    }

    if (!has_pending_join_tree_queries) {
      /* Connection phase complete.
       *
       * - Mark join tree traversal complete and notify application.
       * - Mark the disposition to the node we used to create the connection as
       *   positive so we create an initial route.
       * - Tear the connection-stage resources down.
       */
      ++sys->join_state;
      if (PRAEF_APP_HAS(sys->app, join_tree_traversed_opt))
        (*sys->app->join_tree_traversed_opt)(sys->app);

      RB_FOREACH(node, praef_node_map, &sys->nodes) {
        if (praef_system_net_id_pair_equal(&node->net_id,
                                           sys->join.connect_target)) {
          if (praef_nd_neutral == node->disposition)
            node->disposition = praef_nd_positive;

          break;
        }
      }

      sys->join.connect_target = NULL;
      if (sys->join.connect_mq)
        praef_mq_delete(sys->join.connect_mq);
      sys->join.connect_mq = NULL;
      if (sys->join.connect_out)
        praef_outbox_delete(sys->join.connect_out);
      sys->join.connect_out = NULL;
    } else {
      /* If the interval for join tree query retries has expired, rerun the
       * current query for every node where the join tree traversal is still
       * in-progress.
       *
       * We don't want to do this every frame since there can potentially be a
       * large number of parallel queries.
       */
      if (sys->clock.ticks - sys->join.last_join_tree_query >
          sys->join.join_tree_query_interval) {
        sys->join.last_join_tree_query = sys->clock.ticks;

        RB_FOREACH(node, praef_node_map, &sys->nodes)
          if (~0u != node->join.curr_join_tree_query)
            praef_system_join_query_next_join_tree(sys, node);
      }
    }
  }

  if (sys->join.connect_out)
    PRAEF_OOM_IF_NOT(sys, praef_outbox_flush(sys->join.connect_out));
  if (sys->join.connect_mq)
    praef_mq_update(sys->join.connect_mq);
}

void praef_system_join_recv_msg_join_tree(
  praef_node* from, const PraefMsgJoinTree_t* msg
) {
  PraefMsg_t response;
  OCTET_STRING_t response_data;
  praef_node* against = praef_system_get_node(from->sys, msg->node);
  praef_join_tree_entry* entry, * e;
  unsigned ix = 0;

  memset(&response, 0, sizeof(response));
  memset(&response_data, 0, sizeof(response_data));
  response.present = PraefMsg_PR_jtentry;
  response.choice.jtentry.node = msg->node;
  response.choice.jtentry.offset = msg->offset;

  if (!against) goto send;

  for (ix = 0, entry = STAILQ_FIRST(&against->join.join_tree.children);
       entry && ix < msg->offset;
       ++ix, entry = STAILQ_NEXT(entry, next));
  for (e = entry; e; ++ix, e = STAILQ_NEXT(e, next));

  if (!entry) goto send;

  response.choice.jtentry.data = &response_data;
  response_data.buf = entry->data;
  response_data.size = entry->data_size;

  send:
  response.choice.jtentry.nkeys = ix;
  PRAEF_OOM_IF_NOT(
    from->sys, praef_outbox_append(
      from->router.rpc_out, &response));
}

void praef_node_join_recv_msg_whois(
  praef_node* node, const PraefMsgWhoIs_t* msg
) {
  praef_node* target;

  target = praef_system_get_node(node->sys, msg->node);
  if (!target || PRAEF_BOOTSTRAP_NODE == target->id)
    return;

  praef_system_log(node->sys, "Received whois(%08X) from %08X",
                   msg->node, node->id);

  (*node->bus->unicast)(
    node->bus, &node->net_id, target->join.join_tree.data,
    target->join.join_tree.data_size);
}

/* Note in particular here that join tree query responses are accepted from
 * *unknown* nodes. This is necessary since we won't actually be able to
 * identify the node we initially connected to until we get the message that
 * lead to its creation, unless it happens to be the bootstrap node.
 *
 * It's safe to procede without directly validating authenticity of these
 * messages, since the acceptance messages necessarily form a tree of
 * signatures starting from the bootstrap signature.
 */
void praef_system_join_recv_msg_join_tree_entry(
  praef_system* sys, const PraefMsgJoinTreeEntry_t* msg
) {
  praef_node* against;
  unsigned char data[PRAEF_HLMSG_JOINACCEPT_MAX+1];
  praef_hlmsg nested_msg;

  against = praef_system_get_node(sys, msg->node);
  if (!against) return;

  /* Process the data regardless of whether this is a duplicate. The hash tree
   * will filter dupes out, and there's no other reason to try to do any other
   * filtering here.
   */
  if (msg->data) {
    /* Paranoid bounds check; this should be checked by asn1c's code
     * already.
     */
    if ((unsigned)(msg->data->size + 1) > sizeof(data)) return;

    memcpy(data, msg->data->buf, msg->data->size);
    data[msg->data->size] = 0;
    nested_msg.data = data;
    nested_msg.size = msg->data->size + 1;
    praef_system_state_recv_message(sys, &nested_msg);
  }

  /* Send next message immediately if this was an answer to the previous query
   * for this node and this answer was not "end of list". Otherwise, if this is
   * "end of list" and corresponds to our last query, mark traversal for that
   * node complete.
   */
  if (msg->offset == against->join.curr_join_tree_query) {
    if (msg->data) {
      ++against->join.curr_join_tree_query;
      praef_system_join_query_next_join_tree(sys, against);
    } else {
      against->join.curr_join_tree_query = ~0;
    }
  }
}

static void praef_system_join_query_next_join_tree(
  praef_system* sys, praef_node* against
) {
  PraefMsg_t msg;

  memset(&msg, 0, sizeof(msg));
  msg.present = PraefMsg_PR_jointree;
  msg.choice.jointree.node = against->id;
  msg.choice.jointree.offset = against->join.curr_join_tree_query;
  if (sys->join.connect_out)
    PRAEF_OOM_IF_NOT(sys, praef_outbox_append(sys->join.connect_out, &msg));
}

void praef_system_join_recv_msg_get_network_info(
  praef_system* sys, const PraefMsgGetNetworkInfo_t* msg
) {
  PraefMsg_t response;
  praef_node* bootstrap;
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg response_msg;

  bootstrap = praef_system_get_node(sys, PRAEF_BOOTSTRAP_NODE);
  if (!bootstrap) return;
  if (!praef_system_is_permissible_netid(sys, &msg->retaddr))
    return;

  memset(&response, 0, sizeof(response));
  response.present = PraefMsg_PR_netinfo;
  response.choice.netinfo.salt.buf = sys->join.system_salt;
  response.choice.netinfo.salt.size = sizeof(sys->join.system_salt);
  response.choice.netinfo.saltsig.buf = sys->join.system_salt_sig;
  response.choice.netinfo.saltsig.size = sizeof(sys->join.system_salt_sig);
  response.choice.netinfo.bootstrapkey.buf = bootstrap->pubkey;
  response.choice.netinfo.bootstrapkey.size = PRAEF_PUBKEY_SIZE;
  memcpy(&response.choice.netinfo.bootstrapid,
         &bootstrap->net_id, sizeof(PraefNetworkIdentifierPair_t));
  response_msg.data = data;
  response_msg.size = sizeof(data);
  praef_hlmsg_encoder_set_now(sys->join.minimal_rpc_encoder,
                              sys->clock.monotime);
  praef_hlmsg_encoder_singleton(&response_msg, sys->join.minimal_rpc_encoder,
                                &response);
  (*sys->bus->unicast)(sys->bus, &msg->retaddr,
                       response_msg.data, response_msg.size-1);
}

void praef_system_join_recv_msg_network_info(
  praef_system* sys, const PraefMsgNetworkInfo_t* msg
) {
  praef_node* bootstrap;

  if (sys->join.has_received_network_info) return;

  /* Validate the key/salt pair */
  if (praef_verifier_verify_once(
        sys->verifier,
        msg->bootstrapkey.buf,
        msg->saltsig.buf,
        msg->salt.buf, msg->salt.size)) {
    /* OK, create bootstrap node */
    memcpy(sys->join.system_salt, msg->salt.buf, msg->salt.size);
    memcpy(sys->join.system_salt_sig, msg->saltsig.buf, msg->saltsig.size);

    bootstrap = praef_node_new(sys, 0, PRAEF_BOOTSTRAP_NODE,
                               &msg->bootstrapid,
                               sys->bus, praef_nd_positive,
                               msg->bootstrapkey.buf);
    if (!bootstrap) {
      sys->abnormal_status = praef_ss_oom;
      return;
    }

    if (!praef_system_register_node(sys, bootstrap))
      /* This is the first ever node, should never happen */
      abort();

    sys->join.has_received_network_info = 1;
    /* Triangular routing no longer necessary, since we've gotten a message
     * back from the destination.
     */
    if (sys->join.connect_mq)
      praef_mq_set_triangular(sys->join.connect_mq, 0);
  }
}

static int praef_system_join_verify_jr_signature(
  praef_system* sys, const PraefMsgJoinRequest_t* submsg,
  const unsigned char sig[PRAEF_SIGNATURE_SIZE],
  praef_instant instant
) {
  PraefMsg_t msg;
  praef_hlmsg tmp;
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];

  tmp.data = data;
  tmp.size = sizeof(data);

  /* Reencode the message in normalised form.
   *
   * This will sign the message with *our* public key, but that doesn't really
   * matter since we'll be hitting the verifier manually with the signature
   * provided by the caller, and the signable area excludes both the public key
   * hint and the signature.
   */
  memset(&msg, 0, sizeof(msg));
  msg.present = PraefMsg_PR_joinreq;
  memcpy(&msg.choice.joinreq, submsg, sizeof(PraefMsgJoinRequest_t));
  sys->join.minimal_rpc_serno = 0;
  praef_hlmsg_encoder_set_now(sys->join.minimal_rpc_encoder, instant);
  praef_hlmsg_encoder_singleton(&tmp, sys->join.minimal_rpc_encoder, &msg);
  return praef_verifier_verify_once(
    sys->verifier,
    submsg->publickey.buf,
    sig,
    praef_hlmsg_signable(&tmp),
    praef_hlmsg_signable_sz(&tmp));
}

unsigned praef_system_join_num_live_nodes(praef_system* sys) {
  praef_node* node;
  unsigned count = 0;

  RB_FOREACH(node, praef_node_map, &sys->nodes)
    if (praef_node_is_alive(node))
      ++count;

  return count;
}

static int praef_system_join_is_valid_join_request(
  praef_system* sys, const PraefMsgJoinRequest_t* msg,
  const unsigned char sig[PRAEF_SIGNATURE_SIZE],
  praef_instant instant
) {
  praef_node* bootstrap = praef_system_get_node(sys, PRAEF_BOOTSTRAP_NODE);

  /* Network address MUST match what the system expects */
  if (!praef_system_is_permissible_netid(sys, &msg->identifier))
    return 0;
  /* Public key MUST NOT be the same as that of the bootstrap node */
  if (0 == memcmp(bootstrap->pubkey, msg->publickey.buf,
                  PRAEF_PUBKEY_SIZE))
    return 0;
  /* Signature on hlmsg MUST be valid, and it MUST be in the prescribed
   * normalised encoding.
   */
  if (!praef_system_join_verify_jr_signature(sys, msg, sig, instant))
    return 0;

  /* If the application defines authentication, it MUST have auth data, and
   * it MUST pass the application's test.
   */
  if (PRAEF_APP_HAS(sys->app, is_auth_valid_opt)) {
    if (!msg->auth) return 0;

    if (!(*sys->app->is_auth_valid_opt)(sys->app, msg))
      return 0;
  }

  return 1;
}

void praef_system_join_recv_msg_join_request(
  praef_system* sys, praef_node* node,
  const PraefMsgJoinRequest_t* msg,
  const praef_hlmsg* envelope
) {
  praef_node* bootstrap = praef_system_get_node(sys, PRAEF_BOOTSTRAP_NODE);
  PraefMsg_t accept;

  memset(&accept, 0, sizeof(accept));

  /* If there is no associated node, validate the request and see if we'll be
   * accepting it. If so, create it now, otherwise return. Afterwards (if we
   * didn't reject the request), regardless of whether this created a new node,
   * broadcast the acceptance message.
   */

  /* We can't do anything if this node is not yet a member of the system */
  if (!bootstrap || !sys->join.has_received_network_info ||
      !sys->local_node)
    return;
  /* Special case: The bootstrap node should never send such a message. Do
   * nothing if this came from that node, since there's no Accept message to
   * retransmit.
   */
  if (bootstrap == node) return;

  if (!node) {
    if (!praef_system_join_is_valid_join_request(
          sys, msg, praef_hlmsg_signature(envelope),
          praef_hlmsg_instant(envelope)))
      return;
    /* Refuse request if it would exceed the application's requested rate
     * limit.
     */
    if (sys->join.last_accept - sys->clock.ticks < sys->join.accept_interval)
      return;

    /* Refuse request if accepting this node would put us above the
     * application-suggested node limit.
     */
    if (praef_system_join_num_live_nodes(sys) >= sys->join.max_live_nodes)
      return;

    /* All checks pass, the node is permitted to join.
     *
     * Send an Accept message to everyone, and handle the actual node creation
     * when processing the Accept.
     */
    accept.present = PraefMsg_PR_accept;
    accept.choice.accept.instant = praef_hlmsg_instant(envelope);
    accept.choice.accept.signature.buf = praef_hlmsg_signature(envelope);
    accept.choice.accept.signature.size = PRAEF_SIGNATURE_SIZE;
    memcpy(&accept.choice.accept.request, msg,
           sizeof(PraefMsgJoinRequest_t));
    PRAEF_OOM_IF_NOT(
      sys, praef_outbox_append_singleton(sys->router.ur_out, &accept));
  } else {
    /* Node already exists, just remind it of its Accept message. */
    (*node->bus->unicast)(node->bus, &node->net_id,
                          node->join.join_tree.data,
                          node->join.join_tree.data_size);
  }
}

static int praef_system_join_is_reserved_id(
  praef_system* sys, praef_object_id id
) {
  if (id < 2) return 1;

  if (PRAEF_APP_HAS(sys->app, permit_object_id_opt))
    return !(*sys->app->permit_object_id_opt)(sys->app, id);

  return 0;
}

void praef_system_join_recv_msg_join_accept(
  praef_system* sys, praef_node* from_node,
  const PraefMsgJoinAccept_t* msg,
  const praef_hlmsg* envelope
) {
  praef_keccak_sponge sponge;
  unsigned char local_pubkey[PRAEF_PUBKEY_SIZE];
  praef_object_id id;
  praef_node* new_node, * existing_node;

  /* There are two distinct modes of operation for handling this message.
   *
   * The first case is when the local node is not yet part of the system. In
   * this case, we don't care about from_node (it's probably NULL, but might
   * not be if we happen to be talking to the bootstrap node). If the message
   * is valid *and* refers to the local node's public key, create the local
   * node with the generated id.
   *
   * In the second case, the local node is part of the system. Here we require
   * from_node to be a real node in the system. If the message is valid and
   * refers to a node not already in existence, create the node and send the
   * enveloped message back to the new node. The latter step occurs because
   * the join-request-handling code doesn't actually know the contents of the
   * message it is sending, but we get that here. This additionally has the
   * nice side-effect of performing a NAT hole-punch to the new node in the
   * likely case where it'll be attempting to connect to the local node within
   * a few seconds.
   */

  /* Common validation code.
   *
   * Unlike vanilla join requests, validation failures here are a violation of
   * protocol by from_node, and thus have the side-effect of changing the
   * disposition for that node to negative.
   */
  if (!praef_system_join_is_valid_join_request(
        sys, &msg->request, msg->signature.buf, msg->instant)) {
    if (from_node)
      praef_node_negative(from_node, "Sent Accept with invalid JoinRequest");
    return;
  }

  /* Calculate id for this request */
  praef_sha3_init(&sponge);
  praef_keccak_sponge_absorb(&sponge, sys->join.system_salt,
                             sizeof(sys->join.system_salt));
  praef_keccak_sponge_absorb(&sponge, msg->request.publickey.buf,
                             msg->request.publickey.size);
  id = praef_keccak_sponge_squeeze_integer(&sponge, sizeof(praef_object_id));

  while (praef_system_join_is_reserved_id(sys, id)) ++id;

  praef_signator_pubkey(local_pubkey, sys->signator);
  if (!sys->local_node &&
      0 == memcmp(local_pubkey, msg->request.publickey.buf,
                  PRAEF_PUBKEY_SIZE)) {
    /* This is us. Create local node object and notify application of our new
     * id.
     */
    new_node = praef_node_new(sys, 1, id, sys->self_net_id,
                              &sys->state.loopback, praef_nd_positive,
                              local_pubkey);
    if (!new_node) {
      sys->abnormal_status = praef_ss_oom;
      return;
    }

    if (!praef_system_register_node(sys, new_node)) {
      /* This means we were unfortunate enough to have an id collision with the
       * node we connected with.
       */
      sys->abnormal_status = praef_ss_collision;
      return;
    }

    /* This can't be set until registration completes, since registration could
     * destroy new_node in the rare case where the id collides with the
     * connect-target node.
     */
    sys->local_node = new_node;
    if (PRAEF_APP_HAS(sys->app, acquire_id_opt))
      (*sys->app->acquire_id_opt)(sys->app, id);

    if (from_node)
      praef_system_join_record_in_join_tree(from_node, new_node, envelope);

    ++sys->join_state;
  } else {
    /* If we can't identify the origin of the Accept, discard, as the only
     * expected use of this message is to inform the local node (now a full
     * participant) of other new nodes.
     */
    if (!from_node) return;

    /* If a node with the new id and public key has already joined, there is no
     * action to take, except to ensure that the node is somewhere in the join
     * tree (it might not be if it is the local node and we connected via a
     * non-bootstrap node, since that path won't actually the node that's
     * accepting it into the system).
     */
    existing_node = praef_system_get_node(sys, id);
    if (existing_node && 0 == memcmp(
          existing_node->pubkey, msg->request.publickey.buf,
          PRAEF_PUBKEY_SIZE)) {
      if (!existing_node->join.join_tree.data_size)
        praef_system_join_record_in_join_tree(
          from_node, existing_node, envelope);

      return;
    }

    /* The request is valid, create the new node. The disposition for nodes we
     * accept ourselves always starts out as positive. Other new nodes start
     * neutral; they become positive as we see that other nodes indicate the
     * same.
     *
     * The latter is so that accepts received long after the node actually
     * joined and left, which frequently happens during node joining, will not
     * cause the local node to waste time trying to connect to them.
     */
    new_node = praef_node_new(sys, 0, id, &msg->request.identifier,
                              sys->bus,
                              from_node == sys->local_node?
                              praef_nd_positive : praef_nd_neutral,
                              msg->request.publickey.buf);
    if (!new_node) {
      sys->abnormal_status = praef_ss_oom;
      return;
    }

    if (praef_system_register_node(sys, new_node)) {
      if (PRAEF_APP_HAS(sys->app, discover_node_opt))
        (*sys->app->discover_node_opt)(sys->app, &new_node->net_id, id);

      praef_system_join_record_in_join_tree(from_node, new_node, envelope);

      /* Feed the Accept message back to the new node */
      (*new_node->bus->unicast)(new_node->bus, &new_node->net_id,
                                envelope->data, envelope->size-1);
    }
  }
}

static void praef_system_join_record_in_join_tree(
  praef_node* from, praef_node* new, const praef_hlmsg* msg
) {
  if (msg->size-1 > sizeof(new->join.join_tree.data)) abort();

  memcpy(new->join.join_tree.data, msg->data, msg->size-1);
  new->join.join_tree.data_size = msg->size-1;
  STAILQ_INSERT_TAIL(&from->join.join_tree.children,
                     &new->join.join_tree, next);
}

void praef_system_connect(praef_system* sys,
                          const PraefNetworkIdentifierPair_t* target) {
  sys->join_state = praef_sjs_request_cxn;

  sys->join.connect_target = target;
  sys->join.connect_out = praef_outbox_new(
    praef_hlmsg_encoder_new(praef_htf_rpc_type,
                            sys->signator,
                            &sys->join.minimal_rpc_serno,
                            PRAEF_HLMSG_MTU_MIN, 0),
    PRAEF_HLMSG_MTU_MIN);
  if (sys->join.connect_out)
    sys->join.connect_mq = praef_mq_new(sys->join.connect_out,
                                        sys->bus,
                                        target);

  if (sys->join.connect_mq)
    praef_mq_set_triangular(sys->join.connect_mq, 1);

  PRAEF_OOM_IF_NOT(sys, sys->join.connect_out && sys->join.connect_mq);
}
