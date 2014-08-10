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
#include "keccak.h"
#include "-system.h"
#include "-system-commgr.h"
#include "defs.h"

static void praef_system_commgr_dummy_broadcast(
  praef_message_bus*, const void*, size_t);

int praef_system_commgr_init(praef_system* system,
                             unsigned std_latency,
                             praef_system_profile profile) {
  system->commit_builder = praef_comchain_new();
  system->commit_capture_mq = praef_mq_new(system->cr_out,
                                           &system->commit_capture_bus,
                                           NULL);
  system->commit_capture_bus.broadcast = praef_system_commgr_dummy_broadcast;
  system->last_commit = 0;
  system->commit_interval = std_latency/2 > 0? std_latency/2 : 1;
  system->max_commit_lag = std_latency*8;
  system->max_validated_lag = std_latency*16;
  switch (profile) {
  case praef_sp_lax:
    system->commit_lag_laxness = std_latency;
    system->self_commit_lag_compensation = 65536; /* 1/1 */
    break;

  case praef_sp_strict:
    system->commit_lag_laxness = 0;
    system->self_commit_lag_compensation = 0; /* 0/1 */
    break;
  }

  return system->commit_builder && system->commit_capture_mq;
}

void praef_system_commgr_destroy(praef_system* system) {
  if (system->commit_builder) praef_comchain_delete(system->commit_builder);
  if (system->commit_capture_mq) praef_mq_delete(system->commit_capture_mq);
}

void praef_system_commgr_update(praef_system* system) {
  PraefMsg_t msg;
  unsigned char hash[PRAEF_HASH_SIZE];
  praef_instant last_commit = system->last_commit;

  praef_mq_update(system->commit_capture_mq);

  if (system->clock.monotime >= system->last_commit + system->commit_interval) {
    system->last_commit = system->clock.monotime;
    if (!praef_comchain_create_commit(hash, system->commit_builder,
                                      last_commit + 1,
                                      system->clock.monotime + 1)) {
      system->oom = 1;
      return;
    }

    msg.present = PraefMsg_PR_commit;
    msg.choice.commit.start = last_commit + 1;
    msg.choice.commit.hash.buf = hash;
    msg.choice.commit.hash.size = sizeof(hash);

    system->oom |= !praef_outbox_append(system->ur_out, &msg);
  }
}

void praef_extnode_commgr_update(praef_extnode* node) {
  praef_mq_set_threshold(node->cr_mq, praef_extnode_commgr_horizon(node));

  if (praef_comchain_is_dead(node->comchain) ||
      /* TODO: The below two conditions only apply if node has GRANT */
      praef_comchain_committed(node->comchain) + node->system->max_commit_lag <
        node->system->clock.monotime ||
      praef_comchain_validated(node->comchain) +
        node->system->max_validated_lag < node->system->clock.monotime)
    node->disposition = praef_ed_deny;
}

void praef_extnode_commgr_commit(praef_extnode* node,
                                 praef_instant end,
                                 const PraefMsgCommit_t* commit) {
  node->system->oom |= !praef_comchain_commit(
    node->comchain, commit->start, end+1, commit->hash.buf);
}

void praef_extnode_commgr_receive(praef_extnode* node,
                                  const praef_hlmsg* msg) {
  /* TODO: This hash should actually be calculated by the hash tree and passed
   * into this function from the hash tree manager.
   */
  unsigned char hash[PRAEF_HASH_SIZE];
  praef_keccak_sponge sponge;
  praef_keccak_sponge_init(&sponge, PRAEF_KECCAK_RATE, PRAEF_KECCAK_CAP);
  praef_keccak_sponge_absorb(&sponge, msg->data, msg->size-1);
  praef_keccak_sponge_squeeze(&sponge, hash, sizeof(hash));
  node->system->oom |=
    !praef_comchain_reveal(node->comchain,
                           praef_hlmsg_instant(msg),
                           hash);
}

static void praef_system_commgr_dummy_broadcast(
  praef_message_bus* ccb, const void* data, size_t size
) {
  praef_system* system = UNDOT(praef_system, commit_capture_bus, ccb);
  /* TODO: This hash should actually be calculated by the hash tree and passed
   * into this function from the hash tree manager.
   */
  unsigned char hash[PRAEF_HASH_SIZE];
  praef_keccak_sponge sponge;
  praef_keccak_sponge_init(&sponge, PRAEF_KECCAK_RATE, PRAEF_KECCAK_CAP);
  praef_keccak_sponge_absorb(&sponge, data, size);
  praef_keccak_sponge_squeeze(&sponge, hash, sizeof(hash));

  system->oom |= !praef_comchain_reveal(system->commit_builder,
                                        system->clock.monotime,
                                        hash);
}

praef_instant praef_extnode_commgr_horizon(praef_extnode* node) {
  /* TODO: If node does not have GRANT, return ~0 */

  return praef_comchain_committed(node->comchain) +
    node->system->commit_lag_laxness +
    ((unsigned long long)node->system->self_latency *
     node->system->self_commit_lag_compensation / 65536);
}
