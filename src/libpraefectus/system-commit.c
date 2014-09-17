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

#include "-system.h"

int praef_system_commit_init(praef_system* sys) {
  sys->commit.commit_interval = sys->std_latency/2?
    sys->std_latency/2 : 1;
  sys->commit.max_commit_lag = 8 * sys->std_latency;
  sys->commit.max_validated_lag = 16 * sys->std_latency;
  sys->commit.commit_lag_laxness =
    praef_sp_strict == sys->profile?
    0 : sys->std_latency;
  sys->commit.self_commit_lag_compensation_16 =
    praef_sp_strict == sys->profile?
    0 : 65536; /* 0/1 : 1/1 */

  if (!(sys->commit.commit_builder = praef_comchain_new()))
      return 0;

  return 1;
}

void praef_system_commit_destroy(praef_system* sys) {
  if (sys->commit.commit_builder)
    praef_comchain_delete(sys->commit.commit_builder);
}

int praef_node_commit_init(praef_node* node) {
  if (!(node->commit.comchain = praef_comchain_new()))
    return 0;

  return 1;
}

void praef_node_commit_destroy(praef_node* node) {
  if (node->commit.comchain)
    praef_comchain_delete(node->commit.comchain);
}

void praef_node_commit_observe_message(
  praef_node* node, praef_instant instant,
  const unsigned char hash[PRAEF_HASH_SIZE]
) {
  PRAEF_OOM_IF_NOT(node->sys, praef_comchain_reveal(
                     node->commit.comchain, instant, hash));
}

void praef_node_commit_recv_msg_commit(praef_node* node,
                                       praef_instant end,
                                       const PraefMsgCommit_t* msg) {
  if (end < msg->start) {
    node->disposition = praef_nd_negative;
    return;
  }

  PRAEF_OOM_IF_NOT(node->sys, praef_comchain_commit(
                     node->commit.comchain, msg->start,
                     end+1,  msg->hash.buf));
}

praef_instant praef_node_visibility_threshold(praef_node* node) {
  /* TODO */
  return ~0u;
}

void praef_system_commit_update(praef_system* sys) {
  /* TODO */
}

void praef_node_commit_update(praef_node* node) {
  /* TODO */
}
