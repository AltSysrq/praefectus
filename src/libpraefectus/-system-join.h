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
#include "messages/PraefMsgJoinEndorsement.h"
#include "messages/PraefMsgJoinCommandeerment.h"

typedef struct praef_join_tree_entry_s {
  unsigned char data[128];
  unsigned data_size;

  SLIST_HEAD(,praef_join_tree_entry_s) children;
  SLIST_ENTRY(praef_join_tree_entry_s) next;
} praef_join_tree_entry;

typedef struct {
  unsigned char system_salt[32];
  const PraefNetworkIdentifierPair_t* connect_target;

  unsigned join_tree_query_interval;
  /* Join tree traversal is performed one query at a time per node in the tree,
   * but in parallel for different nodes. (Note that these queries go to the
   * same actual node (the connect target), but are just regarding different
   * nodes known to exist.) join_tree_traversal_complete becomes true when an
   * end-of-branch PraefMsgJoinTreeEntry is received for that node. The index
   * for each query is simply based upon the number of known children for that
   * node so far. The next query is sent immediately upon each response which
   * introduces a new entry to the tree, and is resent every
   * join_tree_query_interval instants in the absence of a response.
   */
  praef_instant last_join_tree_query;
  int join_tree_traversal_complete;

  unsigned num_endorsements_given;
} praef_system_join;

typedef struct {
  int has_route;

  praef_join_tree_entry join_tree;
} praef_node_join;

int praef_system_join_init(praef_system*);
void praef_system_join_destroy(praef_system*);
void praef_system_join_update(praef_system*, unsigned);
int praef_node_join_init(struct praef_node_s*);
void praef_node_join_destroy(struct praef_node_s*);

void praef_node_join_recv_msg_join_tree(
  struct praef_node_s*, const PraefMsgJoinTree_t*);
void praef_node_join_recv_msg_join_tree_entry(
  struct praef_node_s*, const PraefMsgJoinTreeEntry_t*);
void praef_system_join_recv_msg_get_network_info(
  praef_system*, const PraefMsgGetNetworkInfo_t*);
void praef_system_join_recv_msg_network_info(
  praef_system*, const PraefMsgNetworkInfo_t*);
void praef_system_join_recv_join_request(
  praef_system*, const PraefMsgJoinRequest_t*, const praef_hlmsg*);
void praef_system_join_recv_join_endorsement(
  praef_system*, struct praef_node_s*, const PraefMsgJoinEndorsement_t*,
  const praef_hlmsg*);
void praef_system_join_recv_node_commandeerment(
  praef_system*, struct praef_node_s*, const PraefMsgJoinCommandeerment_t*,
  const praef_hlmsg*);

#endif /* LIBPRAEFECTUS__SYSTEM_JOIN_H_ */
