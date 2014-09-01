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
#ifndef LIBPRAEFECTUS__SYSTEM_JOIN_H_
#define LIBPRAEFECTUS__SYSTEM_JOIN_H_

#include "system.h"
#include "messages/PraefMsgJoinTree.h"
#include "messages/PraefMsgJoinTreeEntry.h"
#include "messages/PraefMsgGetNetworkInfo.h"
#include "messages/PraefMsgJoinRequest.h"
#include "messages/PraefMsgJoinAccept.h"

typedef struct praef_join_tree_entry_s {
  unsigned char data[PRAEF_HLMSG_JOINACCEPT_MAX];
  unsigned data_size;

  STAILQ_HEAD(,praef_join_tree_entry_s) children;
  STAILQ_ENTRY(praef_join_tree_entry_s) next;
} praef_join_tree_entry;

typedef struct {
  int has_received_network_info;
  unsigned char bootstrap_key[PRAEF_PUBKEY_SIZE];
  unsigned char system_salt[32];
  unsigned char system_salt_sig[PRAEF_SIGNATURE_SIZE];
  /* These become set when a connection is initiated and are destroyed when the
   * connection completes.
   */
  const PraefNetworkIdentifierPair_t* connect_target;
  praef_outbox* connect_out;
  praef_mq* connect_mq;

  unsigned join_tree_query_interval;
  unsigned accept_interval;
  unsigned max_live_nodes;
  /* Join tree traversal is performed one query at a time per node in the tree,
   * but in parallel for different nodes. (Note that these queries go to the
   * same actual node (the connect target), but are just regarding different
   * nodes known to exist.) join_tree_traversal_complete becomes true when an
   * end-of-branch PraefMsgJoinTreeEntry is received for that node. The index
   * for each query is stored in next_join_tree_query; this is also used to
   * determine whether an incomming response is obsolete. The next query is
   * sent immediately upon each response which introduces a new entry to the
   * tree, and is resent every join_tree_query_interval instants in the absence
   * of a response.
   */
  praef_instant last_join_tree_query;
  int join_tree_traversal_complete;

  praef_instant last_accept;

  praef_hlmsg_encoder* minimal_rpc_encoder;
  /* The serno of the next hlmsg from the minimal RPC encoder.
   *
   * This is a separate field so that sernos can be forced to zero when needed
   * for normalisation purposes.
   */
  praef_advisory_serial_number minimal_rpc_serno;
} praef_system_join;

typedef struct {
  praef_join_tree_entry join_tree;
  unsigned next_join_tree_query;
} praef_node_join;

int praef_system_join_init(praef_system*);
void praef_system_join_destroy(praef_system*);
void praef_system_join_update(praef_system*, unsigned);
int praef_node_join_init(struct praef_node_s*);
void praef_node_join_destroy(struct praef_node_s*);

void praef_system_join_recv_msg_join_tree(
  struct praef_node_s*, const PraefMsgJoinTree_t*);
void praef_system_join_recv_msg_join_tree_entry(
  praef_system*, const PraefMsgJoinTreeEntry_t*);
void praef_system_join_recv_msg_get_network_info(
  praef_system*, const PraefMsgGetNetworkInfo_t*);
void praef_system_join_recv_msg_network_info(
  praef_system*, const PraefMsgNetworkInfo_t*);
void praef_system_join_recv_msg_join_request(
  praef_system*, struct praef_node_s*,
  const PraefMsgJoinRequest_t*, const praef_hlmsg*);
void praef_system_join_recv_msg_join_accept(
  praef_system*, struct praef_node_s*, const PraefMsgJoinAccept_t*,
  const praef_hlmsg*);

#endif /* LIBPRAEFECTUS__SYSTEM_JOIN_H_ */
