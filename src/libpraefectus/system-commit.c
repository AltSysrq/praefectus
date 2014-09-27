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

#include "keccak.h"
#include "-system.h"
#include "messages/PraefMsg.h"
#include "defs.h"

static void praef_system_commit_cr_loopback_broadcast(
  praef_message_bus*, const void*, size_t);

void praef_system_conf_commit_interval(praef_system* sys, unsigned i) {
  sys->commit.commit_interval = i;
}

void praef_system_conf_max_commit_lag(praef_system* sys, unsigned i) {
  sys->commit.max_commit_lag = i;
}

void praef_system_conf_max_validated_lag(praef_system* sys, unsigned i) {
  sys->commit.max_validated_lag = i;
}

void praef_system_conf_commit_lag_laxness(praef_system* sys, unsigned i) {
  sys->commit.commit_lag_laxness = i;
}

void praef_system_conf_self_commit_lag_compensation(praef_system* sys,
                                                    unsigned n,
                                                    unsigned d) {
  sys->commit.self_commit_lag_compensation_16 = n*65536 / d;
}

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

  sys->commit.cr_loopback.broadcast =
    praef_system_commit_cr_loopback_broadcast;

  if (!(sys->commit.commit_builder = praef_comchain_new()))
      return 0;

  if (!(sys->commit.cr_intercept = praef_mq_new(sys->router.cr_out,
                                                &sys->commit.cr_loopback,
                                                NULL)))
    return 0;

  return 1;
}

void praef_system_commit_destroy(praef_system* sys) {
  if (sys->commit.commit_builder)
    praef_comchain_delete(sys->commit.commit_builder);

  if (sys->commit.cr_intercept)
    praef_mq_delete(sys->commit.cr_intercept);
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
    praef_node_negative(node, "Received commit with end (%d) < start (%d)",
                        end, (praef_instant)msg->start);
    return;
  }

  PRAEF_OOM_IF_NOT(node->sys, praef_comchain_commit(
                     node->commit.comchain, msg->start,
                     end+1,  msg->hash.buf));
}

void praef_system_commit_cr_loopback_broadcast(
  praef_message_bus* cr_loopback, const void* data, size_t sz
) {
  const praef_hlmsg hlmsg = { sz + 1, data };
  unsigned char hash[PRAEF_HASH_SIZE];
  praef_keccak_sponge sponge;
  praef_system* sys;

  sys = UNDOT(praef_system, commit,
              UNDOT(praef_system_commit, cr_loopback, cr_loopback));

  praef_sha3_init(&sponge);
  praef_keccak_sponge_absorb(&sponge, data, sz);
  praef_keccak_sponge_squeeze(&sponge, hash, PRAEF_HASH_SIZE);

  PRAEF_OOM_IF_NOT(sys,
                   praef_comchain_reveal(
                     sys->commit.commit_builder,
                     praef_hlmsg_instant(&hlmsg),
                     hash));

  if (praef_comchain_is_dead(sys->commit.commit_builder))
    abort();
}

praef_instant praef_node_visibility_threshold(praef_node* node) {
  praef_node* other;
  praef_instant thresh = praef_comchain_committed(node->commit.comchain);
  unsigned min_latency = ~0u;
  unsigned long long frac;

  if (node == node->sys->local_node)
    return ~0u;
  else if (praef_node_has_deny(node))
    return 0;

  thresh += node->sys->commit.commit_lag_laxness;

  RB_FOREACH(other, praef_node_map, &node->sys->nodes)
    if (other != node->sys->local_node &&
        praef_nd_positive == other->disposition &&
        other->routemgr.latency < min_latency)
      min_latency = other->routemgr.latency;

  if (!~min_latency) min_latency = 0;

  /* Reduce from round-trip to half-one-way */
  min_latency /= 4;
  /* Scale according to settings */
  frac = min_latency;
  frac *= node->sys->commit.self_commit_lag_compensation_16;
  thresh += frac >> 16;

  return thresh;
}

void praef_system_commit_update(praef_system* sys) {
  PraefMsg_t msg;
  unsigned char hash[PRAEF_HASH_SIZE];

  praef_mq_update(sys->commit.cr_intercept);

  if (sys->commit.last_commit < sys->clock.monotime &&
      sys->clock.monotime - sys->commit.last_commit - 1 >=
      sys->commit.commit_interval) {
    if (!praef_comchain_create_commit(hash, sys->commit.commit_builder,
                                      sys->commit.last_commit,
                                      sys->clock.monotime)) {
      praef_system_oom(sys);
      return;
    }

    memset(&msg, 0, sizeof(msg));
    msg.present = PraefMsg_PR_commit;
    msg.choice.commit.start = sys->commit.last_commit;
    msg.choice.commit.hash.buf = hash;
    msg.choice.commit.hash.size = PRAEF_HASH_SIZE;

    /* Since this runs after the router has flushed encoders immediately prior
     * to the next update cycle, we need to encode as a singleton.
     *
     * Back-date the commit by 1 since it is possible for multiple frames to
     * execute within the same monotime instant.
     */
    praef_outbox_set_now(sys->router.ur_out, sys->clock.monotime - 1);
    PRAEF_OOM_IF_NOT(sys, praef_outbox_append_singleton(
                       sys->router.ur_out, &msg));
    praef_outbox_set_now(sys->router.ur_out, sys->clock.monotime);

    sys->commit.last_commit = sys->clock.monotime;
  }
}

void praef_node_commit_update(praef_node* node) {
  praef_instant committed, validated;
  praef_instant systime = node->sys->clock.systime;

  if (node->router.cr_mq)
    praef_mq_set_threshold(node->router.cr_mq,
                           praef_node_visibility_threshold(node));

  if (praef_node_is_in_grace_period(node)) return;

  if (praef_comchain_is_dead(node->commit.comchain)) {
    praef_node_negative(node, "Comchain is broken");
    return;
  }

  committed = praef_comchain_committed(node->commit.comchain);
  validated = praef_comchain_validated(node->commit.comchain);

  if ((committed < systime && systime - committed >
       node->sys->commit.max_commit_lag) ||
      (validated < systime && systime - validated >
       node->sys->commit.max_validated_lag)) {
    /* Don't enforce these if the node doesn't have GRANT or the local node is
     * still joining --- we might just be missing a lot of information.
     */
    if (praef_sjs_connected == node->sys->join_state &&
        praef_node_has_grant(node))
      praef_node_negative(node, "Commit or validated lag exceeded; "
                          "committed = %d, validated = %d, systime = %d",
                          committed, validated, systime);
    return;
  }
}
