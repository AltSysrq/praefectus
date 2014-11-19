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

#include "stdsys.h"

typedef struct {
  praef_app app;
  praef_std_state* stack;
  praef_system* system;
  praef_userdata userdata;
  unsigned (*classify_event)(praef_app*,const praef_event*);
  praef_stdsys_event_vote_t event_vote;
} praef_stdsys;

static int praef_stdsys_create_node(
  praef_app*, praef_object_id);
static praef_instant praef_stdsys_get_node_grant(
  praef_app*, praef_object_id);
static praef_instant praef_stdsys_get_node_deny(
  praef_app*, praef_object_id);
static void praef_stdsys_insert_event(
  praef_app*, praef_event*);
static void praef_stdsys_neutralise_event(
  praef_app*, praef_event*);
static void praef_stdsys_chmod(
  praef_app*, praef_object_id, praef_object_id,
  unsigned, praef_instant);
static int praef_stdsys_has_chmod(
  praef_app*, praef_object_id, praef_object_id,
  unsigned, praef_instant);
static void praef_stdsys_vote(
  praef_app*, praef_object_id,
  praef_object_id, praef_instant,
  praef_event_serial_number);
static void praef_stdsys_advance(
  praef_app*, unsigned);
static void praef_stdsys_noop_apply(
  praef_object*, const praef_event*, praef_userdata);
static unsigned praef_stdsys_all_events_are_pessimistic(
  praef_app* app, const praef_event* evt
) {
  return 0;
}
static int praef_stdsys_event_vote_default(
  praef_app*, const praef_event*, praef_userdata);

praef_app* praef_stdsys_new(praef_std_state* stack) {
  praef_stdsys* this = calloc(1, sizeof(praef_stdsys));
  if (!this) return NULL;

  this->stack = stack;
  this->classify_event = praef_stdsys_all_events_are_pessimistic;
  this->event_vote = praef_stdsys_event_vote_default;
  this->app.create_node_bridge = praef_stdsys_create_node;
  this->app.get_node_grant_bridge = praef_stdsys_get_node_grant;
  this->app.get_node_deny_bridge = praef_stdsys_get_node_deny;
  this->app.insert_event_bridge = praef_stdsys_insert_event;
  this->app.neutralise_event_bridge = praef_stdsys_neutralise_event;
  this->app.chmod_bridge = praef_stdsys_chmod;
  this->app.has_chmod_bridge = praef_stdsys_has_chmod;
  this->app.vote_bridge = praef_stdsys_vote;
  this->app.advance_bridge = praef_stdsys_advance;
  this->app.size = sizeof(praef_app);
  return (praef_app*)this;
}

void praef_stdsys_set_system(praef_app* this, praef_system* sys) {
  ((praef_stdsys*)this)->system = sys;
}

void praef_stdsys_set_userdata(praef_app* this, praef_userdata userdata) {
  ((praef_stdsys*)this)->userdata = userdata;
}

praef_userdata praef_stdsys_userdata(const praef_app* this) {
  return ((const praef_stdsys*)this)->userdata;
}

void praef_stdsys_optimistic_events(praef_app* this,
                                    unsigned (*f)(praef_app*,
                                                  const praef_event*)) {
  ((praef_stdsys*)this)->classify_event = f;
}

void praef_stdsys_event_vote(praef_app* this,
                             praef_stdsys_event_vote_t f) {
  ((praef_stdsys*)this)->event_vote = f;
}

void praef_stdsys_delete(praef_app* this) {
  free(this);
}

#define STACK (((praef_stdsys*)this)->stack)
#define SYSTEM (((praef_stdsys*)this)->system)

static int praef_stdsys_create_node(
  praef_app* this, praef_object_id node
) {
  if (1 != node)
    return praef_metatransactor_add_node(STACK->mtx, node);
  else
    return 1;
}

static praef_instant praef_stdsys_get_node_grant(
  praef_app* this, praef_object_id node
) {
  return praef_metatransactor_get_grant(STACK->mtx, node);
}

static praef_instant praef_stdsys_get_node_deny(
  praef_app* this, praef_object_id node
) {
  return praef_metatransactor_get_deny(STACK->mtx, node);
}

static void praef_stdsys_insert_event(praef_app* this, praef_event* evt) {
  praef_stdsys* stdsys = (praef_stdsys*)this;
  unsigned optimism = (*stdsys->classify_event)(this, evt);
  praef_event* txevt = praef_transactor_put_event(STACK->tx, evt, !!optimism);
  praef_event* dlevt;
  if (!txevt) {
    praef_system_oom(SYSTEM);
    return;
  }

  if (!praef_metatransactor_add_event(STACK->mtx, evt->object, txevt)) {
    praef_system_oom(SYSTEM);
    return;
  }

  if (optimism) {
    dlevt = praef_transactor_deadline(STACK->tx, evt, evt->instant + optimism);
    if (!dlevt) {
      praef_system_oom(SYSTEM);
      return;
    }

    if (!praef_metatransactor_add_event(STACK->mtx, evt->object, dlevt)) {
      praef_system_oom(SYSTEM);
      return;
    }
  }

  if ((*stdsys->event_vote)(this, evt, stdsys->userdata)) {
    if (!praef_system_vote_event(SYSTEM, evt->object, evt->instant,
                                 evt->serial_number)) {
      praef_system_oom(SYSTEM);
      return;
    }
  }
}

static void praef_stdsys_neutralise_event(praef_app* this, praef_event* evt) {
  evt->apply = praef_stdsys_noop_apply;
  praef_context_rewind(STACK->context, evt->instant);
}

static void praef_stdsys_noop_apply(praef_object* target,
                                    const praef_event* evt,
                                    praef_userdata userdata) {
}

static void praef_stdsys_chmod(praef_app* this, praef_object_id target,
                               praef_object_id voter, unsigned mask,
                               praef_instant when) {
  if (!praef_metatransactor_chmod(STACK->mtx,
                                  target, voter, mask, when))
    praef_system_oom(SYSTEM);
}

static int praef_stdsys_has_chmod(praef_app* this, praef_object_id target,
                                  praef_object_id voter, unsigned mask,
                                  praef_instant when) {
  return praef_metatransactor_has_chmod(STACK->mtx, target, voter, mask, when);
}

static void praef_stdsys_vote(praef_app* this,
                              praef_object_id voter,
                              praef_object_id object,
                              praef_instant instant,
                              praef_event_serial_number serno) {
  praef_event* evt = praef_transactor_votefor(
    STACK->tx, object, instant, serno);

  if (!evt) {
    praef_system_oom(SYSTEM);
    return;
  }

  if (!praef_metatransactor_add_event(STACK->mtx, voter, evt))
    praef_system_oom(SYSTEM);
}

static void praef_stdsys_advance(praef_app* this, unsigned delta) {
  praef_std_state_advance(STACK, delta,
                          ((praef_stdsys*)this)->userdata);
}

static int praef_stdsys_event_vote_default(
  praef_app* vthis, const praef_event* evt, praef_userdata _
) {
  praef_stdsys* this = (praef_stdsys*)vthis;
  const praef_clock* clock = praef_system_get_clock(this->system);
  unsigned optimistic;

  optimistic = (*this->classify_event)(vthis, evt);

  return !optimistic ||
    evt->instant > clock->systime ||
    evt->instant - clock->systime <= optimistic;
}
