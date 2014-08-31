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

#include "messages/PraefMsg.h"
#include "system.h"
#include "-system.h"
#include "secure-random.h"

static void praef_system_join_query_next_join_tree(praef_system*, praef_node*);

int praef_system_join_init(praef_system* sys) {
  sys->join.join_tree_query_interval = sys->std_latency;

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
                                NULL,
                                PRAEF_HLMSG_MTU_MIN, 0)))
    return 0;

  return 1;
}

void praef_system_join_destroy(praef_system* sys) {
  praef_hlmsg_encoder_delete(sys->join.minimal_rpc_encoder);
  if (sys->join.connect_out) {
    praef_outbox_delete(sys->join.connect_out);
    praef_mq_delete(sys->join.connect_mq);
  }
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

void praef_system_join_update(praef_system* sys, unsigned et) {
  praef_hlmsg_encoder_set_now(sys->join.minimal_rpc_encoder,
                              sys->clock.monotime);
  if (sys->join.connect_out) {
    praef_outbox_set_now(sys->join.connect_out, sys->clock.monotime);
    praef_mq_update(sys->join.connect_mq);
  }

  /* TODO */
}

void praef_node_join_recv_msg_join_tree(
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
  response_data.buf = against->join.join_tree.data;
  response_data.size = against->join.join_tree.data_size;

  send:
  response.choice.jtentry.nkeys = ix;
  from->sys->oom |= praef_outbox_append(
    from->router.rpc_out, &response);
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
void praef_node_join_recv_msg_join_tree_entry(
  praef_system* sys, const PraefMsgJoinTreeEntry_t* msg
) {
  praef_node* against;
  unsigned char data[129];
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
   * for this node and this answer was not "end of list".
   */
  if (msg->offset == against->join.next_join_tree_query - 1 &&
      msg->data)
    praef_system_join_query_next_join_tree(sys, against);
}

static void praef_system_join_query_next_join_tree(
  praef_system* sys, praef_node* against
) {
  PraefMsg_t msg;

  memset(&msg, 0, sizeof(msg));
  msg.present = PraefMsg_PR_jointree;
  msg.choice.jointree.node = against->id;
  msg.choice.jointree.offset = against->join.next_join_tree_query++;
  sys->oom |= praef_outbox_append(sys->join.connect_out, &msg);
}

void praef_system_join_recv_msg_get_network_info(
  praef_system* sys, const PraefMsgGetNetworkInfo_t* msg
) {
  PraefMsg_t response;
  praef_node* bootstrap;
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg response_msg;

  bootstrap = praef_system_get_node(sys, 1);
  if (!bootstrap) return;

  memset(&response, 0, sizeof(response));
  response.present = PraefMsg_PR_netinfo;
  if (OCTET_STRING_fromBuf(&response.choice.netinfo.salt,
                           (char*)sys->join.system_salt,
                           sizeof(sys->join.system_salt)) ||
      OCTET_STRING_fromBuf(&response.choice.netinfo.saltsig,
                           (char*)sys->join.system_salt_sig,
                           sizeof(sys->join.system_salt_sig)) ||
      OCTET_STRING_fromBuf(&response.choice.netinfo.bootstrapkey,
                           (char*)bootstrap->pubkey,
                           PRAEF_PUBKEY_SIZE)) {
    sys->oom = 1;
    return;
  }

  response_msg.data = data;
  response_msg.size = sizeof(data);
  praef_hlmsg_encoder_singleton(&response_msg, sys->join.minimal_rpc_encoder,
                                &response);
  (*sys->bus->unicast)(sys->bus, &msg->retaddr,
                       response_msg.data, response_msg.size-1);
}

void praef_system_join_recv_msg_network_info(
  praef_system* sys, const PraefMsgNetworkInfo_t* msg
) {
  praef_verifier* verifier = NULL;
  praef_node* bootstrap;

  if (sys->join.has_received_network_info) return;

  /* Validate the key/salt pair */
  verifier = praef_verifier_new();
  if (!verifier) goto oom;
  if (!praef_verifier_assoc(verifier, msg->bootstrapkey.buf, 1))
    goto oom;

  if (praef_verifier_verify(
        verifier, praef_pubkey_hint_of(msg->bootstrapkey.buf),
        msg->saltsig.buf,
        msg->salt.buf, msg->salt.size)) {
    /* OK, create bootstrap node */
    memcpy(sys->join.system_salt, msg->salt.buf, msg->salt.size);
    memcpy(sys->join.system_salt_sig, msg->saltsig.buf, msg->saltsig.size);

    bootstrap = praef_node_new(sys, 1, sys->bus, praef_nd_positive,
                               msg->bootstrapkey.buf);
    if (!bootstrap) goto oom;
    if (!praef_system_register_node(sys, bootstrap))
      /* This is the first ever node, should never happen */
      abort();

    sys->join.has_received_network_info = 1;
  }

  praef_verifier_delete(verifier);
  return;

  oom:
  sys->oom = 1;
  if (verifier) praef_verifier_delete(verifier);
}
