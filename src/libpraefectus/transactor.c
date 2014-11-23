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

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "transactor.h"
#include "defs.h"

/* A transactor is itself a praef_object. This means that we get correct and
 * consistent handling of event addition/redaction/sequencing for free, plus a
 * consistent interface. However, it also makes the operation of the transactor
 * a bit more difficult.
 *
 * Maintaining the transactor as a simple array of states isn't practical,
 * since it would need far too much memory. Use of persistent data structures
 * also doesn't work too well, as it would require error-prone
 * reference-counting (or the introduction of libgc, which I don't want to do
 * from this library).
 *
 * Instead, we maintain only one copy of the state, which gets mutated
 * in-place. Additionally, a journal is kept, each entry knowing how to undo
 * a mutation on the transactor. Thus, on rewind, the transactor walks backward
 * through the journal undoing mutations in reverse the order they were
 * applied. This unfortunately makes rewind O(t), but likely this will not be a
 * substantial problem.
 *
 * In all there are three classes of state to track:
 *
 * - Subordinate events, in particular how many votes they have accrued. This
 *   data is packed into the event wrapper, so the memory is managed by the
 *   containing context.
 *
 * - Node counts, to know how many votes any event needs given its point in
 *   time in order to be accepted. This data is stored within the node-count
 *   event, so the memory is managed by the containing context.
 *
 * - The journal. The journal entries are stored in the events that produce
 *   them, so again the memory is managed by the containing context.
 */

typedef void (*praef_transactor_journal_unapply_t)(
  praef_transactor* transactor, void* this);

/**
 * Information for a single journal entry. This consists solely of the instant
 * at which the entry applies and the function to unapply it. It is assumed
 * that the function knows how to derive the containing object (if any) from
 * its this pointer.
 *
 * Journal entries are only ever accessed LIFO, so an SLIST is sufficient for
 * all operations we will ever need to perform.
 */
typedef struct praef_transactor_journal_entry_s {
  praef_instant when;
  praef_transactor_journal_unapply_t unapply;

  SLIST_ENTRY(praef_transactor_journal_entry_s) prev;
} praef_transactor_journal_entry;

/**
 * Information for the node count. Node count entries are only created when the
 * count changes, so a single entry is valid from its valid_after field up to
 * the next node count entry.
 *
 * While we do sometimes need randomish access to these, there will generally
 * be very few node count entries, and we almost always care about the latest,
 * so an SLIST allows to keep the code simple without hurting performance too
 * much.
 */
typedef struct praef_transactor_node_count_s {
  praef_instant valid_after;
  unsigned count;

  SLIST_ENTRY(praef_transactor_node_count_s) prev;
} praef_transactor_node_count;

/* Events of the same type in the same instant can occur in any order without
 * effect. However, to maintain the same invariant for events of *different*
 * types would require a substantial amount of complex code (eg, a node count
 * change would need to find all events effected and re-check the accept/reject
 * status). Instead, just use the serial number ordering to force them to occur
 * in a partuclar order.
 */
#define SN_MASK       ((praef_event_serial_number)0x3FFFFFFFu)
#define SN_NODE_COUNT ((praef_event_serial_number)0x00000000u)
#define SN_EVENT      ((praef_event_serial_number)0x40000000u)
#define SN_VOTEFOR    ((praef_event_serial_number)0x80000000u)
#define SN_DEADLINE   ((praef_event_serial_number)0xC0000000u)

typedef struct praef_transactor_votefor_event_s {
  praef_event self;

  praef_object_id evt_object;
  praef_instant evt_time;
  praef_event_serial_number evt_sn;

  praef_transactor_journal_entry undo;
} praef_transactor_votefor_event;

typedef struct praef_transactor_node_count_delta_event_s {
  praef_event self;

  signed delta;

  praef_transactor_node_count node_count;
  praef_transactor_journal_entry undo;
} praef_transactor_node_count_delta_event;

typedef struct praef_transactor_wrapped_event_s {
  praef_event self;

  praef_event* delegate;
  praef_event proxy;

  unsigned votes;
  int optimistic;
  int has_been_accepted;
  praef_transactor_journal_entry undo;

  SPLAY_ENTRY(praef_transactor_wrapped_event_s) map;
} praef_transactor_wrapped_event;

typedef struct praef_transactor_deadline_event_s {
  praef_event self;

  praef_object_id evt_object;
  praef_instant evt_time;
  praef_event_serial_number evt_sn;

  praef_transactor_journal_entry undo;
} praef_transactor_deadline_event;

static int praef_compare_transactor_wrapped_event(
  const praef_transactor_wrapped_event* a,
  const praef_transactor_wrapped_event* b
) {
  return praef_compare_event_sequence(a->delegate, b->delegate);
}

SPLAY_HEAD(praef_transactor_wrapped_event_map,
           praef_transactor_wrapped_event_s);
SPLAY_PROTOTYPE(praef_transactor_wrapped_event_map,
                praef_transactor_wrapped_event_s,
                map,
                praef_compare_transactor_wrapped_event);
SPLAY_GENERATE(praef_transactor_wrapped_event_map,
               praef_transactor_wrapped_event_s,
               map,
               praef_compare_transactor_wrapped_event);

struct praef_transactor_s {
  praef_object self;

  praef_context* master, * slave;
  praef_transactor_node_count initial_node_count;
  praef_event_serial_number next_evt_sn;

  SLIST_HEAD(, praef_transactor_journal_entry_s) journal;
  SLIST_HEAD(, praef_transactor_node_count_s) node_count;
  struct praef_transactor_wrapped_event_map events;
};

static void praef_transactor_step(praef_transactor*, praef_userdata);
static void praef_transactor_rewind(praef_transactor*, praef_instant);

praef_transactor* praef_transactor_new(praef_context* slave) {
  praef_context* master;
  praef_transactor* this;

  if (!(master = praef_context_new())) return NULL;
  if (!(this = malloc(sizeof(praef_transactor)))) {
    praef_context_delete(master);
    return NULL;
  }

  this->self.step = (praef_object_step_t)praef_transactor_step;
  this->self.rewind = (praef_object_rewind_t)praef_transactor_rewind;
  this->self.id = 1;
  this->master = master;
  this->slave = slave;
  this->initial_node_count.valid_after = 0;
  this->initial_node_count.count = 1;
  this->next_evt_sn = 0;

  SLIST_INIT(&this->journal);
  SLIST_INIT(&this->node_count);
  SLIST_INSERT_HEAD(&this->node_count, &this->initial_node_count, prev);
  SPLAY_INIT(&this->events);

  praef_context_add_object(this->master, &this->self);

  return this;
}

void praef_transactor_delete(praef_transactor* this) {
  praef_context_delete(this->slave);
  praef_context_delete(this->master);
  free(this);
}

praef_context* praef_transactor_slave(const praef_transactor* this) {
  return this->slave;
}

praef_context* praef_transactor_master(const praef_transactor* this) {
  return this->master;
}

static void praef_transactor_step(praef_transactor* this, praef_userdata _) {
  /* There isn't actually anything for the transactor to do on step ---
   * everything happens in response to events.
   */
}

static void praef_transactor_rewind(praef_transactor* this,
                                    praef_instant when) {
  while (!SLIST_EMPTY(&this->journal) &&
         SLIST_FIRST(&this->journal)->when >= when) {
    (*SLIST_FIRST(&this->journal)->unapply)(
      this, SLIST_FIRST(&this->journal));
    SLIST_REMOVE_HEAD(&this->journal, prev);
  }
}

static praef_transactor_wrapped_event*
praef_transactor_get_wrapped_event(praef_transactor* this,
                                   praef_object_id object,
                                   praef_instant instant,
                                   praef_event_serial_number sn) {
  praef_transactor_wrapped_event example;
  example.delegate = (praef_event*)&example;
  example.self.object = object;
  example.self.instant = instant;
  example.self.serial_number = sn;

  return SPLAY_FIND(praef_transactor_wrapped_event_map,
                    &this->events, &example);
}

static praef_transactor_node_count*
praef_transactor_get_node_count(const praef_transactor* this,
                                praef_instant when) {
  praef_transactor_node_count* count;

  SLIST_FOREACH(count, &this->node_count, prev)
    if (count->valid_after <= when)
      return count;

  abort();
}

static void praef_transactor_accept_reject_event(
  praef_transactor* this,
  praef_transactor_wrapped_event* evt,
  const praef_transactor_node_count* node_count
) {
  int should_be_accepted = (evt->optimistic ||
                            evt->votes*2 >= node_count->count);

  if (should_be_accepted) {
    if (!evt->has_been_accepted) {
      praef_context_add_event(this->slave, &evt->proxy);
      evt->has_been_accepted = 1;
    }
  } else {
    if (evt->has_been_accepted) {
      praef_context_redact_event(this->slave, evt->proxy.object,
                                 evt->proxy.instant, evt->proxy.serial_number);
      evt->has_been_accepted = 0;
    }
  }
}

/******************************** NODE COUNT ******************************/

static void praef_transactor_node_count_delta_apply(
  praef_transactor* tx,
  praef_transactor_node_count_delta_event* evt
) {
  evt->node_count.count = SLIST_FIRST(&tx->node_count)->count + evt->delta;
  SLIST_INSERT_HEAD(&tx->node_count, &evt->node_count, prev);
  SLIST_INSERT_HEAD(&tx->journal, &evt->undo, prev);
}

static void praef_transactor_node_count_delta_unapply(
  praef_transactor* tx,
  praef_transactor_journal_entry* undo,
  praef_userdata _
) {
  SLIST_REMOVE_HEAD(&tx->node_count, prev);
}

praef_event* praef_transactor_node_count_delta(
  praef_transactor* tx, signed delta, praef_instant when
) {
  praef_transactor_node_count_delta_event* evt = malloc(
    sizeof(praef_transactor_node_count_delta_event));
  if (!evt) return NULL;

  evt->self.object = 1;
  evt->self.instant = when;
  evt->self.serial_number = ((tx->next_evt_sn++) & SN_MASK) | SN_NODE_COUNT;
  evt->self.apply =
    (praef_event_apply_t)praef_transactor_node_count_delta_apply;
  evt->self.free = free;
  evt->delta = delta;
  evt->node_count.valid_after = when;
  evt->undo.unapply = (praef_transactor_journal_unapply_t)
    praef_transactor_node_count_delta_unapply;
  evt->undo.when = when;

  return &evt->self;
}

/****************************** VOTEFOR ******************************/

static void praef_transactor_votefor_apply(
  praef_transactor* tx,
  praef_transactor_votefor_event* evt,
  praef_userdata _
) {
  praef_transactor_wrapped_event* target;
  praef_transactor_node_count* node_count;

  target = praef_transactor_get_wrapped_event(
    tx, evt->evt_object, evt->evt_time, evt->evt_sn);
  if (!target) return;

  ++target->votes;
  node_count = praef_transactor_get_node_count(tx, target->self.instant);
  praef_transactor_accept_reject_event(tx, target, node_count);

  SLIST_INSERT_HEAD(&tx->journal, &evt->undo, prev);
}

static void praef_transactor_votefor_unapply(
  praef_transactor* tx,
  praef_transactor_journal_entry* undo
) {
  praef_transactor_votefor_event* evt =
    UNDOT(praef_transactor_votefor_event, undo, undo);
  praef_transactor_wrapped_event* target;
  praef_transactor_node_count* node_count;

  target = praef_transactor_get_wrapped_event(
    tx, evt->evt_object, evt->evt_time, evt->evt_sn);
  if (!target) return;

  --target->votes;
  node_count = praef_transactor_get_node_count(tx, target->self.instant);
  praef_transactor_accept_reject_event(tx, target, node_count);
}

praef_event* praef_transactor_votefor(
  praef_transactor* tx,
  praef_object_id object,
  praef_instant instant,
  praef_event_serial_number serial_number
) {
  praef_transactor_votefor_event* evt;

  if (!(evt = malloc(sizeof(praef_transactor_votefor_event))))
    return NULL;

  evt->self.object = 1;
  evt->self.instant = instant;
  evt->self.serial_number = ((tx->next_evt_sn++) & SN_MASK) | SN_VOTEFOR;
  evt->self.apply = (praef_event_apply_t)praef_transactor_votefor_apply;
  evt->self.free = free;
  evt->evt_object = object;
  evt->evt_time = instant;
  evt->evt_sn = serial_number;
  evt->undo.when = instant;
  evt->undo.unapply = (praef_transactor_journal_unapply_t)
    praef_transactor_votefor_unapply;

  return (praef_event*)evt;
}

/****************************** WRAPPED ******************************/

static void praef_transactor_wrapped_proxy_apply(
  praef_object* obj,
  praef_event* proxy,
  praef_userdata userdata
) {
  praef_event* delegate =
    UNDOT(praef_transactor_wrapped_event, proxy, proxy)->delegate;

  (*delegate->apply)(obj, delegate, userdata);
}

static void praef_transactor_wrapped_proxy_free(void* _) { }

static void praef_transactor_wrapped_apply(
  praef_transactor* tx,
  praef_transactor_wrapped_event* evt,
  praef_userdata _
) {
  praef_transactor_node_count* node_count =
    praef_transactor_get_node_count(tx, evt->self.instant);

  evt->has_been_accepted = 0;
  SPLAY_INSERT(praef_transactor_wrapped_event_map, &tx->events, evt);
  praef_transactor_accept_reject_event(tx, evt, node_count);

  SLIST_INSERT_HEAD(&tx->journal, &evt->undo, prev);
}

static void praef_transactor_wrapped_unapply(
  praef_transactor* tx,
  praef_transactor_journal_entry* undo
) {
  praef_transactor_wrapped_event* evt =
    UNDOT(praef_transactor_wrapped_event, undo, undo);
  if (evt->has_been_accepted)
    praef_context_redact_event(tx->slave, evt->proxy.object,
                               evt->proxy.instant, evt->proxy.serial_number);
  SPLAY_REMOVE(praef_transactor_wrapped_event_map, &tx->events, evt);
}

static void praef_transactor_wrapped_event_free(
  praef_transactor_wrapped_event* evt
) {
  (*evt->delegate->free)((praef_event*)evt->delegate);
  free(evt);
}

praef_event* praef_transactor_put_event(
  praef_transactor* tx,
  praef_event* delegate,
  int optimistic
) {
  praef_transactor_wrapped_event* evt;

  if (!(evt = malloc(sizeof(praef_transactor_wrapped_event))))
    return NULL;

  evt->self.object = 1;
  evt->self.instant = delegate->instant;
  evt->self.serial_number = ((tx->next_evt_sn++) & SN_MASK) | SN_EVENT;
  evt->self.apply = (praef_event_apply_t)praef_transactor_wrapped_apply;
  evt->self.free = (praef_free_t)praef_transactor_wrapped_event_free;
  evt->delegate = delegate;
  evt->proxy.object = delegate->object;
  evt->proxy.instant = delegate->instant;
  evt->proxy.serial_number = delegate->serial_number;
  evt->proxy.apply = (praef_event_apply_t)praef_transactor_wrapped_proxy_apply;
  evt->proxy.free = (praef_free_t)praef_transactor_wrapped_proxy_free;
  evt->votes = 0;
  evt->optimistic = optimistic;
  evt->undo.when = evt->self.instant;
  evt->undo.unapply = (praef_transactor_journal_unapply_t)
    praef_transactor_wrapped_unapply;

  return (praef_event*)evt;
}

/****************************** DEADLINE ******************************/

static void praef_transactor_deadline_apply(
  praef_transactor* tx,
  praef_transactor_deadline_event* evt,
  praef_userdata _
) {
  praef_transactor_wrapped_event* target;

  target = praef_transactor_get_wrapped_event(
    tx, evt->evt_object, evt->evt_time, evt->evt_sn);
  if (!target) return;

  target->optimistic = 0;
  praef_transactor_accept_reject_event(
    tx, target, praef_transactor_get_node_count(tx, target->self.instant));

  SLIST_INSERT_HEAD(&tx->journal, &evt->undo, prev);
}

static void praef_transactor_deadline_unapply(
  praef_transactor* tx,
  praef_transactor_journal_entry* undo
) {
  praef_transactor_deadline_event* evt =
    UNDOT(praef_transactor_deadline_event, undo, undo);
  praef_transactor_wrapped_event* target;

  target = praef_transactor_get_wrapped_event(
    tx, evt->evt_object, evt->evt_time, evt->evt_sn);
  if (!target) return;

  target->optimistic = 1;
  praef_transactor_accept_reject_event(
    tx, target, praef_transactor_get_node_count(tx, target->self.instant));
}

praef_event* praef_transactor_deadline(praef_transactor* tx,
                                       praef_event* target,
                                       praef_instant deadline) {
  praef_transactor_deadline_event* evt;

  if (!(evt = malloc(sizeof(praef_transactor_deadline_event))))
    return NULL;

  evt->self.object = 1;
  evt->self.instant = deadline;
  evt->self.serial_number = ((tx->next_evt_sn++) & SN_MASK) | SN_DEADLINE;
  evt->self.apply = (praef_event_apply_t)praef_transactor_deadline_apply;
  evt->self.free = free;
  evt->evt_object = target->object;
  evt->evt_time = target->instant;
  evt->evt_sn = target->serial_number;
  evt->undo.when = deadline;
  evt->undo.unapply = (praef_transactor_journal_unapply_t)
    praef_transactor_deadline_unapply;

  return (praef_event*)evt;
}
