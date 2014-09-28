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
#include "ack-table.h"
#include "messages/PraefMsg.h"

static int praef_node_ack_is_visible(praef_node* node,
                                     praef_instant instant) {
  return instant < praef_node_visibility_threshold(node);
}

static void praef_system_ack_create_received(PraefMsg_t*,
                                             unsigned char[128],
                                             praef_object_id apropos,
                                             praef_ack_local* table);

void praef_system_conf_direct_ack_interval(praef_system* sys, unsigned i) {
  sys->ack.direct_ack_interval = i;
}

void praef_system_conf_indirect_ack_interval(praef_system* sys, unsigned i) {
  sys->ack.indirect_ack_interval = i;
}

int praef_system_ack_init(praef_system* sys) {
  sys->ack.direct_ack_interval = 4 * sys->std_latency;
  sys->ack.indirect_ack_interval = 16 * sys->std_latency;
  return 1;
}

void praef_system_ack_destroy(praef_system* sys) { }

int praef_node_ack_init(praef_node* node) {
  praef_ack_local_init(&node->ack.al_direct);
  praef_ack_local_init(&node->ack.al_indirect);
  praef_ack_remote_init(&node->ack.ack_remote);
  return 1;
}

void praef_node_ack_destroy(praef_node* node) { }

void praef_node_ack_update(praef_node* node) { }

void praef_node_ack_observe_msg(praef_node* node,
                                const praef_hlmsg* msg,
                                praef_hash_tree_sid sid) {
  praef_ack_local_put(&node->ack.al_direct, msg);
  praef_ack_local_put(&node->ack.al_indirect, msg);
}

void praef_node_ack_recv_msg_received(praef_node* sender,
                                      const PraefMsgReceived_t* msg) {
  const praef_hlmsg* missing[PRAEF_ACK_TABLE_SIZE];
  praef_node* target;
  praef_ack_remote tmp_remote, * ack_remote;
  unsigned i, n;

  target = praef_system_get_node(sender->sys, msg->node);
  if (!target) return;

  if (target == sender->sys->local_node) {
    ack_remote = &target->ack.ack_remote;
  } else {
    praef_ack_remote_init(&tmp_remote);
    ack_remote = &tmp_remote;
  }

  praef_ack_remote_set_base(ack_remote, msg->base, msg->negoff,
                            msg->received.size * 8);
  for (i = 0; i < 8 * (unsigned)msg->received.size; ++i) {
    praef_ack_remote_put(
      ack_remote,
      ((praef_advisory_serial_number)msg->base) + i,
      (msg->received.buf[i/8] >> (i % 8)) & 1);
  }

  n = praef_ack_find_missing(missing, &target->ack.al_direct, ack_remote);
  for (i = 0; i < n; ++i) {
    if (praef_node_ack_is_visible(sender, praef_hlmsg_instant(missing[i]))) {
      (*sender->bus->unicast)(sender->bus, &sender->net_id,
                              missing[i]->data, missing[i]->size-1);
      praef_ack_remote_put(ack_remote, praef_hlmsg_serno(missing[i]), 1);
    }
  }
}

void praef_node_ack_recv_msg_ack(praef_node* node,
                                 const PraefMsgAck_t* msg) {
  /* TODO */
}

void praef_system_ack_update(praef_system* sys) {
  PraefMsg_t msg;
  unsigned char buf[128];
  praef_node* destination, * apropos;

  if (sys->clock.ticks - sys->ack.last_direct_ack >=
      sys->ack.direct_ack_interval) {
    RB_FOREACH(destination, praef_node_map, &sys->nodes) {
      if (destination != sys->local_node &&
          praef_node_is_alive(destination)) {
        praef_system_ack_create_received(&msg, buf, destination->id,
                                         &destination->ack.al_direct);
        PRAEF_OOM_IF_NOT(sys, praef_outbox_append(
                           destination->router.rpc_out, &msg));
      }
    }

    sys->ack.last_direct_ack = sys->clock.ticks;
  }

  if (sys->clock.ticks - sys->ack.last_indirect_ack >=
      sys->ack.indirect_ack_interval) {
    RB_FOREACH(apropos, praef_node_map, &sys->nodes) {
      if (apropos == sys->local_node ||
          !praef_node_is_alive(apropos)) continue;

      praef_system_ack_create_received(&msg, buf, apropos->id,
                                       &apropos->ack.al_indirect);

      RB_FOREACH(destination, praef_node_map, &sys->nodes) {
        if (destination == sys->local_node ||
            destination == apropos ||
            !praef_node_is_alive(destination)) continue;

        PRAEF_OOM_IF_NOT(sys, praef_outbox_append(
                           destination->router.rpc_out, &msg));
      }
    }

    sys->ack.last_indirect_ack = sys->clock.ticks;
  }
}

static void praef_system_ack_create_received(PraefMsg_t* msg,
                                             unsigned char buf[128],
                                             praef_object_id apropos,
                                             praef_ack_local* table) {
  unsigned i, n;

  memset(msg, 0, sizeof(PraefMsg_t));
  msg->present = PraefMsg_PR_received;

  msg->choice.received.node = apropos;

  if (table->delta_end == table->delta_start) {
    /* Special case, since this would wander off of the constraints by 1. */
    msg->choice.received.negoff = 1023;
    msg->choice.received.base = table->delta_start;
    msg->choice.received.received.buf = buf;
    msg->choice.received.received.size = 1;
    buf[0] = 0;
  } else {
    /* This is always <= 1023, since these two would be equal otherwise. */
    msg->choice.received.negoff = table->delta_start - table->base;
    msg->choice.received.base = table->delta_start;
    msg->choice.received.received.buf = buf;
    msg->choice.received.received.size = n =
      (table->delta_end - table->delta_start + 7) / 8;

    memset(buf, 0, n);
    for (i = 0; i < n*8; ++i)
      buf[i/8] |=
        (!!praef_ack_local_get(table, i + table->delta_start))
        << (i%8);
  }

  table->delta_start = table->delta_end;
}
