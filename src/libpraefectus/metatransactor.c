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

#include <assert.h>

#include "common.h"
#include "event.h"
#include "object.h"
#include "context.h"
#include "metatransactor.h"
#include "defs.h"

/* The nature of the metatransactor makes it somewhat hard to think about, even
 * though it is in principle a simple component. Therefore, the whole design
 * will be described here, in one place.
 *
 * The state of a node can be thought of as simply its current status bits,
 * plus a list of future events and meta-events. This representation, however,
 * would require an ever-growing table of past states, and incur undue
 * redaction/reinsertion cycles on rewind. It also would reuqire the object to
 * be able to insert/redact meta-events within its own meta-context during
 * event application or object stepping, neither of which are supported
 * operations.
 *
 * Instead, a more implicit model is used. The status bits for a node are
 * indicated by the instants at which they become set, allowing the determining
 * of whether an event was accepted at any arbitrary point in time. Meta-events
 * can thus be unconditionally added to the meta-context, and simply determine
 * for themselves whether they should apply when invoked.
 *
 * Additionally, each node tracks the set of normal events it has received, in
 * standard order, plus a bit indicating whether it has been sent downstream. A
 * cursor into this set is held, always pointing at the first event after the
 * current time of the node. As a node advances time, it accepts/redacts events
 * via this cursor as necessary. Events which are added to the metatransactor
 * are immediately inserted into this set and accepted if appropriate. (Ie,
 * normal events are not wrapped in meta-events.)
 *
 * Management of node-count-delta events for downstream is also somewhat
 * unnatural, since they need to act on chmod events. This is handled by
 * additionally storing the delta incr/decr events on the node itself, if they
 * have been given to the connection. Thus, a node can redact them when it
 * rewinds before them, and the chmod event when applied can identify whether
 * the delta has already been applied. For the sake of simplicitly, chmod
 * events only affect one bit of the node status each --- in the rare event
 * that anyone wants to set no bits or set both bits together, zero or two
 * actual events are produced. By using the bit to set as the "serial number"
 * of the chmod meta-events, the built-in (object,instant,serial_number) triple
 * becomes sufficient to identify the events within the context.
 *
 * The chmod meta-events, though they undergo voting similarly to what is
 * provided by the transactor, cannot be simply passed to a transactor, as
 * votes may or may not count based upon the stati of the participating
 * nodes. Instead, each chmod event statefully tracks the nodes that have
 * nominally voted for it (which is append-only at this level) so it can count
 * votes at the time it is actually applied.
 */

#define GRANT_BIT 0
#define DENY_BIT 1

typedef struct praef_metatransactor_node_event_s {
  praef_event self;
  praef_event* delegate;
  int has_been_accepted;
  SPLAY_ENTRY(praef_metatransactor_node_event_s) sequence;
  TAILQ_ENTRY(praef_metatransactor_node_event_s) subsequent;
} praef_metatransactor_node_event;

static int praef_compare_metatransactor_node_event(
  const praef_metatransactor_node_event* a,
  const praef_metatransactor_node_event* b
) {
  return praef_compare_event_sequence(a->delegate, b->delegate);
}

SPLAY_HEAD(praef_metatransactor_node_event_sequence,
           praef_metatransactor_node_event_s);
SPLAY_PROTOTYPE(praef_metatransactor_node_event_sequence,
                praef_metatransactor_node_event_s, sequence,
                praef_compare_metatransactor_node_event);
SPLAY_GENERATE(praef_metatransactor_node_event_sequence,
               praef_metatransactor_node_event_s, sequence,
               praef_compare_metatransactor_node_event);

typedef struct praef_metatransactor_node_s {
  praef_object self;
  praef_metatransactor* owner;
  /**
   * The current (actual) time of the state of this node.
   */
  praef_instant now;
  /**
   * Instants at which the two status bits became set. Values >= now indicate
   * that the bits are not currently set. They are forced to ~0 if in the
   * future when the node is rewound.
   */
  praef_instant bits_set[2];
  /**
   * The events sent to the underlying connection corresponding to setting the
   * GRANT and DENY bits, respectively, if they have in fact been sent. NULL
   * otherwise. Note that the bits may be set while these are NULL if the DENY
   * bit becomes set first. They are redacted from the underlying connection if
   * the object is rewound to or before the time of the corresponding bit's
   * becomming set. (Note that they do not necessarily correspond to the chmod
   * event that set the bits_set field; if a later chmod sets the bit to an
   * earlier time, it will retain the event from the chmod that originally set
   * the bit.)
   */
  praef_event* node_incr_evt, * node_decr_evt;

  /**
   * Splay tree for efficient computation of event order.
   */
  struct praef_metatransactor_node_event_sequence event_sequence;
  /**
   * List in ascending order by event.
   */
  TAILQ_HEAD(,praef_metatransactor_node_event_s) events;
  /**
   * The next event that has yet to be examined, or NULL if all events are in
   * the past.
   */
  praef_metatransactor_node_event* cursor;

  SLIST_ENTRY(praef_metatransactor_node_s) next;
} praef_metatransactor_node;

/**
 * Information on a single vote for a particular chmod event.
 */
typedef struct praef_metatransactor_chmod_vote_s {
  /**
   * The node which has nominally voted for this event. Note that the vote does
   * not count unless the node is ALIVE at the time this event is to apply.
   */
  praef_metatransactor_node* voter;
  SLIST_ENTRY(praef_metatransactor_chmod_vote_s) next;
} praef_metatransactor_chmod_vote;

/**
 * The chmod meta-event, affecting the status bits of a node.
 */
typedef struct praef_metatransactor_chmod_evt_s {
  praef_event self;
  /**
   * Either GRANT_BIT or DENY_BIT (an index into the bits_set array). The bit
   * which is to be set by this event.
   */
  unsigned bit_to_set;
  /**
   * The raw node count delta event to send downstream if this event takes
   * effect.
   */
  praef_event* node_delta_evt;
  /**
   * node_delta_evt wrapped so that free() does nothing.
   */
  praef_event node_delta_proxy;
  SLIST_HEAD(,praef_metatransactor_chmod_vote_s) votes;
} praef_metatransactor_chmod_evt;

struct praef_metatransactor_s {
  praef_context* context;
  praef_metatransactor_cxn* cxn;

  SLIST_HEAD(,praef_metatransactor_node_s) nodes;
};

/****************************** Top-Level ******************************/

static praef_metatransactor_node* praef_metatransactor_node_new(
  praef_metatransactor*, praef_object_id);
static void praef_metatransactor_node_delete(praef_metatransactor_node*);

praef_metatransactor* praef_metatransactor_new(praef_metatransactor_cxn* cxn) {
  praef_metatransactor* this = malloc(sizeof(praef_metatransactor));
  praef_metatransactor_node* bootstrap;
  if (!this) return NULL;

  this->cxn = cxn;
  SLIST_INIT(&this->nodes);
  if (!(this->context = praef_context_new())) goto fail;

  bootstrap = praef_metatransactor_node_new(
    this, PRAEF_BOOTSTRAP_NODE);
  if (!bootstrap) goto fail;

  bootstrap->bits_set[GRANT_BIT] = 0;
  return this;

  fail:
  praef_metatransactor_delete(this);
  return NULL;
}

void praef_metatransactor_delete(praef_metatransactor* this) {
  praef_metatransactor_node* node, * tmp;

  SLIST_FOREACH_SAFE(node, &this->nodes, next, tmp)
    praef_metatransactor_node_delete(node);
  praef_context_delete(this->context);
  free(this);
}

void praef_metatransactor_advance(praef_metatransactor* this, unsigned amt) {
  praef_context_advance(this->context, amt, NULL);
}

/****************************** Node ******************************/

static void praef_metatransactor_node_step(
  praef_metatransactor_node*, praef_userdata);
static void praef_metatransactor_node_rewind(
  praef_metatransactor_node*, praef_instant);

static praef_metatransactor_node* praef_metatransactor_node_new(
  praef_metatransactor* owner, praef_object_id id
) {
  praef_metatransactor_node* this = malloc(sizeof(praef_metatransactor_node));
  if (!this) return NULL;

  this->owner = owner;
  this->self.id = id;
  this->self.step = (praef_object_step_t)praef_metatransactor_node_step;
  this->self.rewind = (praef_object_rewind_t)praef_metatransactor_node_rewind;
  this->bits_set[0] = ~0u;
  this->bits_set[1] = ~0u;
  this->node_incr_evt = NULL;
  this->node_decr_evt = NULL;
  SPLAY_INIT(&this->event_sequence);
  TAILQ_INIT(&this->events);
  this->cursor = NULL;

  if (praef_context_add_object(owner->context, (praef_object*)this)) {
    free(this);
    return NULL;
  }

  SLIST_INSERT_HEAD(&owner->nodes, this, next);
  return this;
}

static void praef_metatransactor_node_delete(praef_metatransactor_node* this) {
  praef_metatransactor_node_event* evt, * tmp;

  TAILQ_FOREACH_SAFE(evt, &this->events, subsequent, tmp) {
    (*evt->delegate->free)(evt->delegate);
    free(evt);
  }

  free(this);
}

static inline praef_metatransactor_node_status
praef_metatransactor_get_node_status(praef_metatransactor_node* n) {
  return (n->bits_set[0] < n->now) | ((n->bits_set[1] < n->now) << 1);
}

static inline int praef_metatransactor_node_alive(
  praef_metatransactor_node* n
) {
  return PRAEF_METATRANSACTOR_NS_ALIVE ==
    praef_metatransactor_get_node_status(n);
}

static void praef_metatransactor_node_step(
  praef_metatransactor_node* this, praef_userdata _
) {
  void (*process)(praef_metatransactor_cxn*, praef_event*);
  int alive;

  ++this->now;

  /* Alter accepted/rejected state of events as necessary */
  alive = praef_metatransactor_node_alive(this);
  process = (alive? this->owner->cxn->accept : this->owner->cxn->redact);
  while (this->cursor && this->cursor->self.instant == this->now) {
    if (alive != this->cursor->has_been_accepted) {
      (*process)(this->owner->cxn, (praef_event*)this->cursor);
      this->cursor->has_been_accepted = alive;
    }

    this->cursor = TAILQ_NEXT(this->cursor, subsequent);
  }
}

static praef_metatransactor_node_event*
praef_metatransactor_node_first_event_after(
  const praef_metatransactor_node* this, praef_instant when
) {
  praef_metatransactor_node_event* evt, * next;

  next = SPLAY_ROOT(&this->event_sequence);
  if (!next) return NULL;
  do {
    evt = next;
    if (when > evt->self.instant) next = SPLAY_RIGHT(evt, sequence);
    else                          next = SPLAY_LEFT(evt, sequence);
  } while (next);

  /* The above loop will either terminate on the last event before the chosen
   * time, or the first event after it, depending on the structure of the
   * tree. Check for the former case and move one element forward.
   */
  if (evt->self.instant < when) evt = TAILQ_NEXT(evt, subsequent);

  return evt;
}

static void praef_metatransactor_node_rewind(
  praef_metatransactor_node* this, praef_instant then
) {
  this->now = then;
  this->cursor = praef_metatransactor_node_first_event_after(this, then);

  /* Clear status bits if not in the past */
  if (this->bits_set[GRANT_BIT] >= then) {
    if (this->node_incr_evt)
      (*this->owner->cxn->redact)(this->owner->cxn,
                                  (praef_event*)this->node_incr_evt);

    this->bits_set[GRANT_BIT] = ~0;
    this->node_incr_evt = NULL;
  }

  if (this->bits_set[DENY_BIT] >= then) {
    if (this->node_decr_evt)
      (*this->owner->cxn->redact)(this->owner->cxn,
                                  (praef_event*)this->node_decr_evt);

    this->bits_set[DENY_BIT] = ~0;
    this->node_decr_evt = NULL;
  }

  /* The bootstrap node always has the GRANT bit set */
  if (PRAEF_BOOTSTRAP_NODE == this->self.id)
    this->bits_set[GRANT_BIT] = 0;
}

int praef_metatransactor_add_node(praef_metatransactor* this,
                                  praef_object_id node_id) {
  return !!praef_metatransactor_node_new(this, node_id);
}

/****************************** Nested Events ******************************/

static void praef_metatransactor_node_event_apply(
  praef_object* target,
  const praef_metatransactor_node_event* this,
  praef_userdata userdata
) {
  (*this->delegate->apply)(target, this->delegate, userdata);
}

static void praef_metatransactor_node_event_free(void* _) { }

int praef_metatransactor_add_event(praef_metatransactor* this,
                                   praef_object_id node_id,
                                   praef_event* delegate) {
  praef_metatransactor_node_event* evt = NULL, * preceding, * next;
  praef_metatransactor_node* node;

  node = (praef_metatransactor_node*)praef_context_get_object(
    this->context, node_id);
  if (!node) goto fail;

  evt = malloc(sizeof(praef_metatransactor_node_event));
  if (!evt) goto fail;
  evt->self.apply = (praef_event_apply_t)praef_metatransactor_node_event_apply;
  evt->self.free = (praef_free_t)praef_metatransactor_node_event_free;
  evt->self.object = delegate->object;
  evt->self.instant = delegate->instant;
  evt->self.serial_number = delegate->serial_number;
  evt->delegate = delegate;
  evt->has_been_accepted = 0;

  /* Try to insert into the node's events */
  if (SPLAY_INSERT(praef_metatransactor_node_event_sequence,
                   &node->event_sequence, evt))
    /* Conflict */
    goto fail;

  /* Insert into the sorted list */
  preceding = SPLAY_LEFT(evt, sequence);
  if (preceding) {
    while ((next = SPLAY_RIGHT(preceding, sequence)))
      preceding = next;
    TAILQ_INSERT_AFTER(&node->events, preceding, evt, subsequent);
  } else {
    /* No prior event, so this is the new head */
    TAILQ_INSERT_HEAD(&node->events, evt, subsequent);
  }

  if (evt->self.instant > node->now) {
    /* In the future. The cursor may need to be updated, but the event need not
     * be accepted right now.
     */
    if (!node->cursor)
      /* No events are yet ahead of the cursor, so this becomes the cursor */
      node->cursor = evt;
    else if (praef_compare_metatransactor_node_event(evt, node->cursor) < 0)
      /* This event preceds the former cursor, but is still in the future wrt
       * the node, so this becomes the new cursor.
       */
      node->cursor = evt;
  } else {
    /* The cursor certainly does not need to point to this event, since the
     * event is in the past wrt the node. However, if the node was alive at
     * that time, this event needs to be accepted now.
     */
    if (evt->self.instant > node->bits_set[GRANT_BIT] &&
        evt->self.instant <= node->bits_set[DENY_BIT]) {
      (*this->cxn->accept)(this->cxn, (praef_event*)evt);
      evt->has_been_accepted = 1;
    }
  }

  return 1;

  fail:
  if (evt) free(evt);
  free(delegate);
  return 0;
}

/****************************** Chmod ******************************/

static void praef_metatransactor_chmod_apply(
  praef_metatransactor_node*,
  const praef_metatransactor_chmod_evt*,
  praef_userdata);
static void praef_metatransactor_chmod_free(praef_metatransactor_chmod_evt*);
static void praef_metatransactor_chmod_proxy_apply(
  praef_object*, const praef_event*, praef_userdata);
static void praef_metatransactor_chmod_proxy_free(void*);

static praef_metatransactor_chmod_evt* praef_metatransactor_chmod_new(
  praef_metatransactor* owner,
  unsigned bit_to_set,
  praef_object_id node,
  praef_instant instant
) {
  praef_metatransactor_chmod_evt* this =
    malloc(sizeof(praef_metatransactor_chmod_evt));
  if (!this) return NULL;

  this->self.instant = instant;
  this->self.object = node;
  this->self.serial_number = bit_to_set;
  this->self.apply = (praef_event_apply_t)praef_metatransactor_chmod_apply;
  this->self.free = (praef_free_t)praef_metatransactor_chmod_free;
  this->bit_to_set = bit_to_set;
  this->node_delta_evt = (*owner->cxn->node_count_delta)(
    owner->cxn, GRANT_BIT == bit_to_set? +1 : -1, instant);
  if (!this->node_delta_evt) {
    free(this);
    return NULL;
  }

  this->node_delta_proxy.object = this->node_delta_evt->object;
  this->node_delta_proxy.instant = this->node_delta_evt->instant;
  this->node_delta_proxy.serial_number = this->node_delta_evt->serial_number;
  this->node_delta_proxy.apply =
    (praef_event_apply_t)praef_metatransactor_chmod_proxy_apply;
  this->node_delta_proxy.free = praef_metatransactor_chmod_proxy_free;
  SLIST_INIT(&this->votes);

  return this;
}

static void praef_metatransactor_chmod_apply(
  praef_metatransactor_node* target,
  const praef_metatransactor_chmod_evt* this,
  praef_userdata _
) {
  praef_metatransactor_node* node;
  praef_metatransactor_chmod_vote* vote;
  unsigned votes = 0, possible_voters = 0;

  /* Count the number of nodes eligible for voting */
  SLIST_FOREACH(node, &target->owner->nodes, next)
    if (PRAEF_METATRANSACTOR_NS_ALIVE ==
        praef_metatransactor_get_node_status(node))
      ++possible_voters;

  /* Count the votes received */
  SLIST_FOREACH(vote, &this->votes, next)
    if (PRAEF_METATRANSACTOR_NS_ALIVE ==
        praef_metatransactor_get_node_status(vote->voter))
      ++votes;

  /* Vote carries if at least 50% of eligible voters agree */
  if (votes*2 >= possible_voters) {
    /* If this event actually has an effect on the node count, send an event
     * downstream. Since chmod events of the same bit in the same instant are
     * considered identical, we can safely assume that there is no possible
     * interference with our bit, so the case where bits_set[bit_to_set]==now
     * will never happen. We include the actual now in the alreaty-set range to
     * account for the bootstrep node gaining GRANT spontaneously at instant
     * zero.
     */
    if (GRANT_BIT == this->bit_to_set &&
        target->bits_set[GRANT_BIT] > target->now) {
      assert(!target->node_incr_evt);
      target->node_incr_evt = (praef_event*)&this->node_delta_proxy;
      (*target->owner->cxn->accept)(target->owner->cxn,
                                    (praef_event*)&this->node_delta_proxy);
    } else if (DENY_BIT == this->bit_to_set &&
               target->bits_set[DENY_BIT] > target->now) {
      assert(!target->node_decr_evt);
      target->node_decr_evt = (praef_event*)&this->node_delta_proxy;
      (*target->owner->cxn->accept)(target->owner->cxn,
                                    (praef_event*)&this->node_delta_proxy);
    }

    /* The new time of the bit being set is the time of this event, if this
     * event is earlier than the old time. (In the current design, this only
     * happens if the bit is not set at all, but doing the test this way makes
     * the code more robust against future changes, and is simpler anyway.)
     */
    if (this->self.instant < target->bits_set[this->bit_to_set])
      target->bits_set[this->bit_to_set] = this->self.instant;
  }
}

static void praef_metatransactor_chmod_free(
  praef_metatransactor_chmod_evt* evt
) {
  praef_metatransactor_chmod_vote* vote, * tmp;

  SLIST_FOREACH_SAFE(vote, &evt->votes, next, tmp)
    free(vote);

  (*evt->node_delta_evt->free)(evt->node_delta_evt);
  free(evt);
}

static void praef_metatransactor_chmod_proxy_apply(
  praef_object* target,
  const praef_event* proxy,
  praef_userdata userdata
) {
  praef_metatransactor_chmod_evt* this = UNDOT(
    praef_metatransactor_chmod_evt, node_delta_proxy, proxy);
  (*this->node_delta_evt->apply)(target, this->node_delta_evt, userdata);
}

static void praef_metatransactor_chmod_proxy_free(void* _) { }

int praef_metatransactor_chmod(praef_metatransactor* this,
                               praef_object_id target_id,
                               praef_object_id voter_id,
                               praef_metatransactor_node_status mask,
                               praef_instant instant) {
  praef_metatransactor_node* target, * voter;
  praef_metatransactor_chmod_evt* chmod;
  praef_metatransactor_chmod_vote* vote;
  unsigned bit_to_set;
  target = (praef_metatransactor_node*)
    praef_context_get_object(this->context, target_id);
  voter = (praef_metatransactor_node*)
    praef_context_get_object(this->context, voter_id);

  if (!target || !voter) return 0;

  switch (mask) {
  case PRAEF_METATRANSACTOR_NS_GRANT: bit_to_set = GRANT_BIT; break;
  case PRAEF_METATRANSACTOR_NS_DENY:  bit_to_set = DENY_BIT;  break;
  default: return 0;
  }

  /* See if such an event already exists */
  chmod = (praef_metatransactor_chmod_evt*)
    praef_context_get_event(this->context, target_id, instant, bit_to_set);
  if (!chmod) {
    /* Event does not exist yet, so create it */
    chmod = praef_metatransactor_chmod_new(this, bit_to_set,
                                           target_id, instant);
    if (!chmod) return 0;
    praef_context_add_event(this->context, (praef_event*)chmod);
  }

  /* See if the voter has already cast this vote. If so, return silently. */
  SLIST_FOREACH(vote, &chmod->votes, next)
    if (voter == vote->voter)
      return 1;

  /* Node has not yet voted, so add its vote */
  vote = malloc(sizeof(praef_metatransactor_chmod_vote));
  if (!vote) return 0;
  vote->voter = voter;
  SLIST_INSERT_HEAD(&chmod->votes, vote, next);
  praef_context_rewind(this->context, instant);
  return 1;
}

int praef_metatransactor_has_chmod(praef_metatransactor* this,
                                   praef_object_id target_id,
                                   praef_object_id voter_id,
                                   praef_metatransactor_node_status mask,
                                   praef_instant instant) {
  praef_metatransactor_node* target, * voter;
  praef_metatransactor_chmod_evt* chmod;
  praef_metatransactor_chmod_vote* vote;
  unsigned bit_to_set;
  target = (praef_metatransactor_node*)
    praef_context_get_object(this->context, target_id);
  voter = (praef_metatransactor_node*)
    praef_context_get_object(this->context, voter_id);

  if (!target || !voter) return 0;

  switch (mask) {
  case PRAEF_METATRANSACTOR_NS_GRANT: bit_to_set = GRANT_BIT; break;
  case PRAEF_METATRANSACTOR_NS_DENY:  bit_to_set = DENY_BIT;  break;
  default: return 0;
  }

  /* See if such an event already exists */
  chmod = (praef_metatransactor_chmod_evt*)
    praef_context_get_event(this->context, target_id, instant, bit_to_set);
  if (!chmod) return 0;

  /* See if the voter has already cast this vote. If so, return silently. */
  SLIST_FOREACH(vote, &chmod->votes, next)
    if (voter == vote->voter)
      return 1;

  return 0;
}

praef_instant praef_metatransactor_get_grant(praef_metatransactor* this,
                                             praef_object_id node) {
  return ((praef_metatransactor_node*)praef_context_get_object(
            this->context, node))->bits_set[GRANT_BIT];
}

praef_instant praef_metatransactor_get_deny(praef_metatransactor* this,
                                            praef_object_id node) {
  return ((praef_metatransactor_node*)praef_context_get_object(
            this->context, node))->bits_set[DENY_BIT];
}
