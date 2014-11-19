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

#include "test.h"

#include <libpraefectus/context.h>
#include <libpraefectus/transactor.h>

defsuite(libpraefectus_transactor);

static praef_context* slave;
static praef_context* master;
static praef_transactor* tx;

#define OBJECT_ID ((praef_object_id)42)
static struct object {
  praef_object self;

  int states[256];
  praef_instant now;
} object;

static void object_step(struct object* this, praef_userdata _) {
  ++this->now;
  this->states[this->now+1] = this->states[this->now];
}

static void object_rewind(struct object* this, praef_instant when) {
  this->now = when;
  this->states[this->now+1] = this->states[this->now];
}

defsetup {
  slave = praef_context_new();
  tx = praef_transactor_new(slave);
  master = praef_transactor_master(tx);

  memset(&object, 0, sizeof(object));
  object.self.id = OBJECT_ID;
  object.self.step = (praef_object_step_t)object_step;
  object.self.rewind = (praef_object_rewind_t)object_rewind;
  praef_context_add_object(slave, (praef_object*)&object);
}

defteardown {
  praef_transactor_delete(tx);
}

static praef_event* mkevt(praef_instant when, void (*apply)()) {
  static praef_event_serial_number next_sn = 0;
  praef_event* evt = malloc(sizeof(praef_event));
  evt->object = OBJECT_ID;
  evt->instant = when;
  evt->serial_number = next_sn++;
  evt->apply = (praef_event_apply_t)apply;
  evt->free = free;
  return evt;
}

typedef struct {
  praef_event* a, * b, * delegate;
} evtpair;

static evtpair put_pes_evt(praef_instant when, void (*apply)()) {
  praef_event* evt = mkevt(when, apply);
  praef_event* txevt = praef_transactor_put_event(tx, evt, 0);
  evtpair pair = { txevt, NULL, evt };

  praef_context_add_event(master, txevt);
  return pair;
}

static evtpair put_opt_evt(praef_instant when,
                           praef_instant deadline,
                           void (*apply)()) {
  praef_event* evt = mkevt(when, apply);
  praef_event* wevt = praef_transactor_put_event(tx, evt, 1);
  praef_event* devt = praef_transactor_deadline(tx, evt, deadline);
  evtpair pair = { wevt, devt, evt };

  praef_context_add_event(master, wevt);
  praef_context_add_event(master, devt);
  return pair;
}

static evtpair nodecount(praef_instant when, signed delta) {
  praef_event* evt = praef_transactor_node_count_delta(tx, delta, when);
  evtpair pair = { evt, NULL };

  praef_context_add_event(master, evt);
  return pair;
}

static evtpair votefor(evtpair pair) {
  praef_event* evt = praef_transactor_votefor(
    tx, pair.delegate->object, pair.delegate->instant,
    pair.delegate->serial_number);
  evtpair ret = { evt, NULL };

  praef_context_add_event(master, evt);
  return ret;
}

static void redact(evtpair pair) {
  praef_context_redact_event(
    master, pair.a->object, pair.a->instant, pair.a->serial_number);
  if (pair.b)
    praef_context_redact_event(
      master, pair.b->object, pair.b->instant, pair.b->serial_number);
}

static void step(unsigned amt) {
  praef_context_advance(master, amt, NULL);
  praef_context_advance(slave, amt, NULL);
}

deftest(contained_contexts) {
  ck_assert_ptr_eq(slave, praef_transactor_slave(tx));
  ck_assert_ptr_ne(slave, praef_transactor_master(tx));
  ck_assert(!!praef_transactor_master(tx));
}

deftest(pessimistic_event_not_applied_without_votes) {
  int applied = 0;

  nodecount(0, 10);
  put_pes_evt(1, lambdav((), applied = 1));
  step(2);
  ck_assert_int_eq(0, applied);
}

deftest(optimistic_event_applied_until_deadline) {
  nodecount(0, 10);
  put_opt_evt(1, 3, lambdav((), object.states[object.now+1] = 1));
  step(2);
  ck_assert_int_eq(1, object.states[2]);
  step(2);
  ck_assert_int_eq(0, object.states[2]);
}

deftest(pessimistic_event_applied_one_after_votes) {
  evtpair evt;

  nodecount(0, 4);
  evt = put_pes_evt(1, lambdav((), ++object.states[object.now+1]));
  step(2);
  ck_assert_int_eq(0, object.states[2]);
  votefor(evt);
  step(0);
  ck_assert_int_eq(0, object.states[2]);
  votefor(evt);
  step(0);
  ck_assert_int_eq(1, object.states[2]);
  votefor(evt);
  step(0);
  ck_assert_int_eq(1, object.states[2]);
}

deftest(pessimistic_event_applied_after_node_count_reduction) {
  evtpair evt;

  nodecount(0, 10);
  evt = put_pes_evt(1, lambdav((), ++object.states[object.now+1]));
  step(2);
  ck_assert_int_eq(0, object.states[2]);
  votefor(evt);
  votefor(evt);
  step(0);
  ck_assert_int_eq(0, object.states[2]);
  nodecount(1, -6);
  step(0);
  ck_assert_int_eq(1, object.states[2]);
}

deftest(optimistic_event_not_redacted_at_deadline_if_has_votes) {
  evtpair evt;

  nodecount(0, 4);
  evt = put_opt_evt(1, 3, lambdav((), ++object.states[object.now+1]));
  step(2);
  ck_assert_int_eq(1, object.states[2]);
  votefor(evt);
  votefor(evt);
  step(2);
  ck_assert_int_eq(1, object.states[2]);
}

deftest(event_redacted_after_retroactive_node_count_increase) {
  evtpair evt;

  nodecount(0, 4);
  evt = put_pes_evt(1, lambdav((), ++object.states[object.now+1]));
  votefor(evt);
  votefor(evt);
  step(2);
  ck_assert_int_eq(1, object.states[2]);
  nodecount(1, +1);
  step(0);
  ck_assert_int_eq(0, object.states[2]);
}

deftest(event_resurrected_if_gains_votes_after_deadline) {
  evtpair evt;

  nodecount(0, 3);
  evt = put_opt_evt(1, 3, lambdav((), object.states[object.now+1] = 1));
  step(2);
  ck_assert_int_eq(1, object.states[2]);
  step(2);
  ck_assert_int_eq(0, object.states[2]);
  votefor(evt);
  votefor(evt);
  step(1);
  ck_assert_int_eq(1, object.states[2]);
}


deftest(event_redacted_after_vote_redaction) {
  evtpair evt, votea, voteb;

  nodecount(0, 4);
  evt = put_pes_evt(1, lambdav((), ++object.states[object.now+1]));
  votefor(evt);
  votea = votefor(evt);
  voteb = votefor(evt);
  step(2);
  ck_assert_int_eq(1, object.states[2]);
  redact(votea);
  step(0);
  ck_assert_int_eq(1, object.states[2]);
  redact(voteb);
  step(0);
  ck_assert_int_eq(0, object.states[2]);
}

deftest(event_redacted_after_node_decrease_redaction) {
  evtpair evt, node_count;

  nodecount(0, 5);
  node_count = nodecount(1, -1);
  evt = put_pes_evt(1, lambdav((), ++object.states[object.now+1]));
  votefor(evt);
  votefor(evt);
  step(2);
  ck_assert_int_eq(1, object.states[2]);
  redact(node_count);
  step(0);
  ck_assert_int_eq(0, object.states[2]);
}

deftest(event_unredacted_after_node_increase_redaction) {
  evtpair evt, node_count;

  nodecount(0, 4);
  evt = put_pes_evt(1, lambdav((), ++object.states[object.now+1]));
  votefor(evt);
  votefor(evt);
  step(2);
  ck_assert_int_eq(1, object.states[2]);
  node_count = nodecount(1, +1);
  step(0);
  ck_assert_int_eq(0, object.states[2]);
  redact(node_count);
  step(0);
  ck_assert_int_eq(1, object.states[2]);
}

deftest(event_removed_if_redacted) {
  evtpair evt;

  nodecount(0, 4);
  evt = put_pes_evt(1, lambdav((), ++object.states[object.now+1]));
  votefor(evt);
  votefor(evt);
  step(2);
  ck_assert_int_eq(1, object.states[2]);
  redact(evt);
  step(0);
  ck_assert_int_eq(0, object.states[2]);
}

deftest(event_never_applied_if_redacted_before_acceptance) {
  evtpair evt;

  nodecount(0, 4);
  evt = put_opt_evt(1, 10, lambdav((), ++object.states[object.now+1]));
  redact(evt);
  step(2);
  ck_assert_int_eq(0, object.states[2]);
}

deftest(later_node_count_increase_does_not_affect_earlier_acceptance) {
  evtpair evt;

  nodecount(0, 2);
  evt = put_pes_evt(1, lambdav((), ++object.states[object.now+1]));
  nodecount(2, +10);
  step(3);
  ck_assert_int_eq(0, object.states[3]);
  votefor(evt);
  step(0);
  ck_assert_int_eq(1, object.states[3]);
}

deftest(later_node_count_decrease_does_not_affect_earlier_rejection) {
  evtpair evt;

  nodecount(0, 5);
  evt = put_pes_evt(1, lambdav((), ++object.states[object.now+1]));
  step(3);
  ck_assert_int_eq(0, object.states[3]);
  nodecount(2, -1);
  votefor(evt);
  votefor(evt);
  step(0);
  ck_assert_int_eq(0, object.states[3]);
}

deftest(retroactive_node_count_adjustments_stack) {
  evtpair evt;

  nodecount(0, 4);
  evt = put_pes_evt(10, lambdav((), ++object.states[object.now+1]));
  votefor(evt);
  votefor(evt);
  step(11);
  ck_assert_int_eq(1, object.states[11]);
  nodecount(1, +1);
  step(0);
  ck_assert_int_eq(0, object.states[11]);
  nodecount(5, -1);
  step(0);
  ck_assert_int_eq(1, object.states[11]);
  nodecount(4, +1);
  step(0);
  ck_assert_int_eq(0, object.states[11]);
}

deftest(doesnt_crash_if_head_event_removed_but_deadline_preserved) {
  praef_event* evt = mkevt(1, NULL);
  praef_event* devt = praef_transactor_deadline(tx, evt, 5);

  nodecount(0, 4);
  praef_context_add_event(master, devt);
  step(10);

  (*evt->free)(evt);
}
