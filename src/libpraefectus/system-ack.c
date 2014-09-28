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

RB_GENERATE(praef_node_ack_discont_queue, praef_node_ack_discont_s,
            tree, praef_compare_node_ack_discont)

int praef_compare_node_ack_discont(const praef_node_ack_discont* a,
                                   const praef_node_ack_discont* b) {
  return (a->serno > b->serno) - (a->serno < b->serno);
}

static int praef_node_ack_is_visible(praef_node* node,
                                     praef_instant instant) {
  return instant < praef_node_visibility_threshold(node);
}

static int praef_system_ack_is_public(praef_system* sys,
                                      praef_instant instant) {
  return sys->clock.monotime >= sys->commit.public_visibility_lag &&
    instant <= sys->clock.monotime - sys->commit.public_visibility_lag;
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

void praef_system_conf_linear_ack_interval(praef_system* sys, unsigned i) {
  sys->ack.linear_ack_interval = i;
}

void praef_system_conf_linear_ack_max_xmit(praef_system* sys, unsigned i) {
  sys->ack.linear_ack_max_xmit = i;
}

int praef_system_ack_init(praef_system* sys) {
  sys->ack.direct_ack_interval = 4 * sys->std_latency;
  sys->ack.indirect_ack_interval = 16 * sys->std_latency;
  sys->ack.linear_ack_interval = 2 * sys->std_latency;
  sys->ack.linear_ack_max_xmit = 4;

  sys->ack.ack_sids_cap = 256;
  sys->ack.ack_sids = malloc(sizeof(unsigned) * sys->ack.ack_sids_cap);
  memset(sys->ack.ack_sids, -1, sizeof(unsigned) * sys->ack.ack_sids_cap);
  if (!sys->ack.ack_sids)
    return 0;
  return 1;
}

void praef_system_ack_destroy(praef_system* sys) {
  free(sys->ack.ack_sids);
}

int praef_node_ack_init(praef_node* node) {
  praef_ack_local_init(&node->ack.al_direct);
  praef_ack_local_init(&node->ack.al_indirect);
  praef_ack_remote_init(&node->ack.ack_remote);
  RB_INIT(&node->ack.discont_queue);
  return 1;
}

void praef_node_ack_destroy(praef_node* node) {
  praef_node_ack_discont* d, * tmp;

  for (d = RB_MIN(praef_node_ack_discont_queue, &node->ack.discont_queue);
       d; d = tmp) {
    tmp = RB_NEXT(praef_node_ack_discont_queue, &node->ack.discont_queue, d);
    RB_REMOVE(praef_node_ack_discont_queue, &node->ack.discont_queue, d);
    free(d);
  }
}

void praef_node_ack_update(praef_node* node) {
  PraefMsg_t msg;
  praef_node_ack_discont* discont;

  if (node != node->sys->local_node) {
    /* Dequeue any new contiguous serial numbers for linear ack */
    while ((discont = RB_MIN(praef_node_ack_discont_queue,
                             &node->ack.discont_queue)) &&
           node->ack.max_ack_out == discont->serno) {
      RB_REMOVE(praef_node_ack_discont_queue, &node->ack.discont_queue, discont);
      free(discont);
      ++node->ack.max_ack_out;
    }

    if (node->sys->clock.ticks - node->ack.last_linear_ack >=
        node->sys->ack.linear_ack_interval &&
        praef_nd_positive == node->disposition) {
      memset(&msg, 0, sizeof(PraefMsg_t));
      msg.present = PraefMsg_PR_ack;
      msg.choice.ack.recipient = node->id;
      msg.choice.ack.max = node->ack.max_ack_out;
      PRAEF_OOM_IF_NOT(node->sys, praef_outbox_append(
                         node->router.rpc_out, &msg));
      node->ack.last_linear_ack = node->sys->clock.ticks;
    }
  }
}

void praef_node_ack_observe_msg(praef_node* node,
                                const praef_hlmsg* msg,
                                praef_hash_tree_sid sid) {
  unsigned* new_ack_sids, old_cap;
  praef_node_ack_discont* discont;
  praef_advisory_serial_number serno;

  praef_ack_local_put(&node->ack.al_direct, msg);
  praef_ack_local_put(&node->ack.al_indirect, msg);

  serno = praef_hlmsg_serno(msg);
  if (node == node->sys->local_node) {
    if (serno >= node->sys->ack.ack_sids_cap) {
      old_cap = node->sys->ack.ack_sids_cap;
      while (serno >= node->sys->ack.ack_sids_cap)
        node->sys->ack.ack_sids_cap *= 2;

      new_ack_sids = realloc(node->sys->ack.ack_sids,
                             sizeof(unsigned) * node->sys->ack.ack_sids_cap);
      if (!new_ack_sids) {
        praef_system_oom(node->sys);
        return;
      }

      node->sys->ack.ack_sids = new_ack_sids;
      memset(new_ack_sids + old_cap, -1,
             (node->sys->ack.ack_sids_cap - old_cap) * sizeof(unsigned));
    }

    node->sys->ack.ack_sids[serno] = sid;
  } else {
    /* Add to the queue of discontiguous serial numbers if not already there */
    discont = malloc(sizeof(praef_node_ack_discont));
    if (!discont) {
      praef_system_oom(node->sys);
      return;
    }

    discont->serno = serno;
    if (RB_FIND(praef_node_ack_discont_queue, &node->ack.discont_queue,
                discont)) {
      /* Already present */
      free(discont);
    } else {
      RB_INSERT(praef_node_ack_discont_queue, &node->ack.discont_queue,
                discont);
    }
  }
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
  praef_advisory_serial_number serno;
  unsigned off;
  praef_hash_tree_objref obj;

  if (!node->sys->local_node ||
      msg->recipient != node->sys->local_node->id)
    /* Not actually intended for us */
    return;

  serno = msg->max;
  if (serno < node->ack.max_ack_in)
    /* Obsolete */
    return;

  /* OK, retransmit any permissible packets as necessary. Stop if we encounter
   * an invisible instant.
   *
   * Only consider public messages as visible, to minimise the bandwidth that
   * this would waste otherwise (ie, we don't want to be continuously
   * retransmitting in-flight packets).
   */
  node->ack.max_ack_in = serno;
  for (off = 1; off <= node->sys->ack.linear_ack_max_xmit &&
         serno + off < node->sys->ack.ack_sids_cap; ++off) {
    if (~node->sys->ack.ack_sids[serno+off] &&
        praef_hash_tree_get_id(&obj, node->sys->state.hash_tree,
                               node->sys->ack.ack_sids[serno+off]) &&
        praef_system_ack_is_public(node->sys, obj.instant)) {
      praef_system_log(node->sys,
                       "%08X: Retransmitting serno %d to %08X\n",
                       node->sys->local_node->id,
                       serno+off,
                       node->id);
      (*node->bus->unicast)(node->bus, &node->net_id, obj.data, obj.size);
    }
  }
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
