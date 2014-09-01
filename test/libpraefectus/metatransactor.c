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

#include <libpraefectus/metatransactor.h>

#define NS_GRANT PRAEF_METATRANSACTOR_NS_GRANT
#define NS_DENY  PRAEF_METATRANSACTOR_NS_DENY

defsuite(libpraefectus_metatransactor);

static praef_metatransactor* that;

static void cxn_accept_wrapper(praef_metatransactor_cxn*, praef_event*);
static void cxn_redact_wrapper(praef_metatransactor_cxn*, praef_event*);
static praef_event* cxn_node_count_delta_wrapper(
  praef_metatransactor_cxn*, signed, praef_instant);

static const praef_metatransactor_cxn cxn = {
  .accept = cxn_accept_wrapper,
  .redact = cxn_redact_wrapper,
  .node_count_delta = cxn_node_count_delta_wrapper,
};

static void (*cxn_accept)(praef_event*);
static void (*cxn_redact)(praef_event*);
static praef_event* (*cxn_node_count_delta)(signed, praef_instant);

static void cxn_accept_wrapper(praef_metatransactor_cxn* c, praef_event* e)  {
  ck_assert_ptr_eq(&cxn, c);
  ck_assert(!!e);

  ck_assert_msg(!!cxn_accept, "Unexpected call to cxn::accept");
  (*cxn_accept)(e);
}

static void cxn_redact_wrapper(praef_metatransactor_cxn* c, praef_event* e) {
  ck_assert_ptr_eq(&cxn, c);
  ck_assert(!!e);

  ck_assert_msg(!!cxn_redact, "Unexpected call to cxn::redact");
  (*cxn_redact)(e);
}

static praef_event* cxn_node_count_delta_wrapper(
  praef_metatransactor_cxn* c, signed delta, praef_instant instant
) {
  ck_assert_ptr_eq(&cxn, c);
  ck_assert_int_ne(0, delta);

  ck_assert_msg(!!cxn_node_count_delta, "Unexpected call to "
                "cxn::node_count_delta");
  return (*cxn_node_count_delta)(delta, instant);
}

static void invoke_apply(praef_event* evt) {
  (*evt->apply)(NULL, evt, NULL);
}

#define CLONE(x) ((typeof(x)*)do_clone(&x, sizeof(x)))
static inline void* do_clone(const void* m, size_t sz) {
  void* ret = malloc(sz);
  memcpy(ret, m, sz);
  return ret;
}

defsetup {
  cxn_accept = NULL;
  cxn_redact = NULL;
  cxn_node_count_delta = NULL;
  that = praef_metatransactor_new((praef_metatransactor_cxn*)&cxn);
}

defteardown {
  praef_metatransactor_delete(that);
}

deftest(accepts_events_for_bootstrap_node) {
  int applied = 0;
  praef_event* cloned_evt;
  praef_event evt = {
    .object = 42,
    .instant = 1,
    .serial_number = 0,
    .apply = lambdav((praef_object* _, const praef_event* this,
                      praef_userdata ud),
                     ck_assert_ptr_eq(cloned_evt, this);
                     ck_assert(!applied);
                     applied = 1),
    .free = free,
  };
  cloned_evt = CLONE(evt);

  cxn_accept = invoke_apply;
  praef_metatransactor_add_event(that, PRAEF_BOOTSTRAP_NODE,
                                 cloned_evt);
  praef_metatransactor_advance(that, 2);
  ck_assert(applied);
}

deftest(wont_create_duplicate_node) {
  ck_assert_int_eq(0, praef_metatransactor_add_node(
                     that, PRAEF_BOOTSTRAP_NODE));
}

deftest(node_can_join) {
  /* Node 2 introduces an event at time 2, and petitions to gain GRANT at time
   * 1. We advance to time 3 and verify that the event was not accepted. Then,
   * node 1 retroactively agrees to the petition, and the event is accepted.
   */
  unsigned node_count = 1;
  int accepted = 0;
  praef_event evt = {
    .apply = lambdav((praef_object* obj, const praef_event* this,
                      praef_userdata _),
                     ck_assert(!accepted);
                     ck_assert_int_eq(2, node_count);
                     accepted = 1),
    .free = free,
    .object = 42,
    .instant = 3,
    .serial_number = 0,
  };
  praef_event node_count_delta_evt = {
    .apply = lambdav((praef_object* obj, const praef_event* this,
                      praef_userdata _),
                     ck_assert_int_eq(1, node_count);
                     ++node_count),
    .free = free,
    .object = 1,
    .instant = 2,
    .serial_number = 0,
  };

  cxn_accept = invoke_apply;
  praef_event* node_count_delta(signed amt, praef_instant instant) {
    ck_assert_int_eq(+1, amt);
    ck_assert_int_eq(2, instant);
    return CLONE(node_count_delta_evt);
  }
  cxn_node_count_delta = node_count_delta;

  ck_assert(praef_metatransactor_add_node(that, 2));
  ck_assert(praef_metatransactor_add_event(that, 2, CLONE(evt)));
  ck_assert(praef_metatransactor_chmod(that, 2, 2, NS_GRANT, 2));
  praef_metatransactor_advance(that, 3);
  ck_assert_int_eq(1, node_count);
  ck_assert_int_eq(0, accepted);

  ck_assert(praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 2));
  praef_metatransactor_advance(that, 0);
  ck_assert_int_eq(2, node_count);
  ck_assert(accepted);
}

/******************************************************************************
 * Tests below here use less strict events that just allow for state
 * verification.
 */

/* A bitmask of events which have been accepted. */
static unsigned events_accepted;
static unsigned node_count;
defsetup {
  events_accepted = 0;
  node_count = 1;
}
defteardown { }

typedef struct {
  praef_event self;
  unsigned mask;
  signed node_count_delta;
} basic_event;

/* Basic event apply actually applies if userdata is NULL, and redacts
 * otherwise. Hackish, but this is a test, so oh well.
 *
 * (We can't just look at the event in the cxn methods, since it is wrapped by
 * a proxy internal to the metatransactor.)
 */
static void basic_event_apply(praef_object* _, const basic_event* evt,
                              praef_userdata redact) {
  if (redact) {
    ck_assert_int_eq(evt->mask, events_accepted & evt->mask);
    events_accepted &=~ evt->mask;
    node_count -= evt->node_count_delta;
  } else {
    ck_assert_int_eq(0, events_accepted & evt->mask);
    events_accepted |= evt->mask;
    node_count += evt->node_count_delta;
  }
}

static praef_event* bevent(praef_instant when, unsigned bit) {
  basic_event evt = {
    { .object = 42,
      .instant = when,
      .serial_number = bit,
      .apply = (praef_event_apply_t)basic_event_apply,
      .free = free },
    1 << bit, 0
  };
  return (praef_event*)CLONE(evt);
}

static void basic_event_accept(praef_event* evt) {
  (*evt->apply)(NULL, evt, /* accept */ NULL);
}

static void basic_event_redact(praef_event* evt) {
  (*evt->apply)(NULL, evt, /* redact */ evt);
}

static praef_event* basic_node_count_delta(signed delta, praef_instant when) {
  basic_event evt = {
    { .object = 1,
      .instant = when,
      .serial_number = 0,
      .apply = (praef_event_apply_t)basic_event_apply,
      .free = free },
    0, delta,
  };
  return (praef_event*)CLONE(evt);
}

static void with_basic_events() {
  cxn_accept = basic_event_accept;
  cxn_redact = basic_event_redact;
  cxn_node_count_delta = basic_node_count_delta;
}

deftest(node_can_be_killed) {
  /* Node 2 comes into existence and petitions to gain GRANT at time 1 and to
   * DENY node 1 at time 2. Node 1 adds an event at time 3. Time is advanced to
   * 4; the node count should still be 1, and the event accepted. Node 1 then
   * retroactively agrees with the GRANT petition, leading to its own death and
   * the redaction of its event.
   */
  with_basic_events();
  praef_metatransactor_add_node(that, 2);
  praef_metatransactor_chmod(that, 2, 2, NS_GRANT, 1);
  praef_metatransactor_chmod(that, 1, 2, NS_DENY, 2);
  praef_metatransactor_add_event(that, 1, bevent(3, 0));
  praef_metatransactor_add_event(that, 2, bevent(3, 1));
  praef_metatransactor_advance(that, 4);
  ck_assert_int_eq(1, node_count);
  ck_assert_int_eq(0x1, events_accepted);

  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 1);
  praef_metatransactor_advance(that, 0);
  ck_assert_int_eq(1, node_count);
  ck_assert_int_eq(0x2, events_accepted);
}

deftest(bit_changes_have_no_effect_until_next_instant) {
  /* Nodes 2 and 3 come into existence, and both petition to gain GRANT at time
   * 1 and to DENY node 1 at time 1. Node 1 accepts both GRANTs. All three
   * nodes add events at times 1 and 2. The DENYs should not have any effect
   * becaus the nodes just gained GRANT that instant; similarly, the events by
   * 2 and 3 at time 1 do not get accepted, for the same reason. Howevr, 2 and
   * 3 do gain GRANT (allowing all events at time 2 to be accepted) because
   * they do not count as potential voters at time 1.
   */
  with_basic_events();
  praef_metatransactor_add_node(that, 2);
  praef_metatransactor_add_node(that, 3);
  praef_metatransactor_chmod(that, 2, 2, NS_GRANT, 1);
  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 1);
  praef_metatransactor_chmod(that, 3, 3, NS_GRANT, 1);
  praef_metatransactor_chmod(that, 3, 1, NS_GRANT, 1);
  praef_metatransactor_chmod(that, 1, 2, NS_DENY, 1);
  praef_metatransactor_chmod(that, 1, 3, NS_DENY, 1);
  praef_metatransactor_add_event(that, 1, bevent(1, 0));
  praef_metatransactor_add_event(that, 1, bevent(2, 1));
  praef_metatransactor_add_event(that, 2, bevent(1, 2));
  praef_metatransactor_add_event(that, 2, bevent(2, 3));
  praef_metatransactor_add_event(that, 3, bevent(1, 4));
  praef_metatransactor_add_event(that, 3, bevent(2, 5));
  praef_metatransactor_advance(that, 3);
  ck_assert_int_eq(3, node_count);
  ck_assert_int_eq(/* 0010.1011 */ 0x2B, events_accepted);
}

deftest(retroactive_node_addition_can_reverse_subsequent_vote) {
  /* Nodes 2, 3, and 4 come into existence. Node 1 votes for node 2 to gain
   * GRANT at time 2, and for 4 to gain GRANT at time 1. Nodes 2 and 3
   * introduce events at time 3, and time is advanced to 4. The event for node
   * 2 is applied, but not for 3. Node 1 then retroactively votes for 3 to gain
   * GRANT at time 1. This reverses the vote to give GRANT to node 2, since
   * node 3 is now an eligble voter at time 2, and did not cast such a
   * vote. This means that node 3's event is accepted, but not node 2's.
   */
  with_basic_events();
  praef_metatransactor_add_node(that, 2);
  praef_metatransactor_add_node(that, 3);
  praef_metatransactor_add_node(that, 4);
  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 2);
  praef_metatransactor_chmod(that, 4, 1, NS_GRANT, 1);
  praef_metatransactor_add_event(that, 2, bevent(3, 0));
  praef_metatransactor_add_event(that, 3, bevent(3, 1));
  praef_metatransactor_advance(that, 4);
  ck_assert_int_eq(3, node_count);
  ck_assert_int_eq(0x1, events_accepted);

  praef_metatransactor_chmod(that, 3, 1, NS_GRANT, 1);
  praef_metatransactor_advance(that, 0);
  ck_assert_int_eq(3, node_count);
  ck_assert_int_eq(0x2, events_accepted);
}

deftest(past_event_in_alive_node_immediately_accepted) {
  with_basic_events();
  praef_metatransactor_advance(that, 10);
  praef_metatransactor_add_event(that, 1, bevent(1, 0));
  ck_assert_int_eq(1, events_accepted);
}

deftest(current_event_in_alive_node_immediately_accepted) {
  with_basic_events();
  praef_metatransactor_advance(that, 10);
  praef_metatransactor_add_event(that, 1, bevent(10, 0));
  ck_assert_int_eq(1, events_accepted);
}

deftest(past_event_insertion_uses_past_status) {
  /* Node 2 comes into existence, and adds an event at time 1, and votes to
   * DENY node 1 at time 4. Node 1 votes to GRANT node 2 at time 2, and adds an
   * event at time 5. Both nodes add an event at time 3. All event adds occur
   * *after* the chmods have been processed and time advanced to 6. No
   * advance(0) is required, since the events are processed immediately.
   */
  with_basic_events();
  praef_metatransactor_add_node(that, 2);
  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 2);
  praef_metatransactor_chmod(that, 1, 2, NS_DENY, 4);
  praef_metatransactor_advance(that, 6);
  ck_assert_int_eq(1, node_count);

  praef_metatransactor_add_event(that, 2, bevent(1, 0)); /* Before GRANT */
  praef_metatransactor_add_event(that, 2, bevent(3, 1)); /* After GRANT */
  praef_metatransactor_add_event(that, 1, bevent(3, 2)); /* Before DENY */
  praef_metatransactor_add_event(that, 1, bevent(5, 3)); /* After DENY */
  ck_assert_int_eq(0x6, events_accepted);
}

deftest(out_of_order_future_events_handled_correctly) {
  with_basic_events();
  praef_metatransactor_add_event(that, 1, bevent(10, 0));
  praef_metatransactor_add_event(that, 1, bevent(1, 1));
  praef_metatransactor_add_event(that, 1, bevent(2, 2));
  praef_metatransactor_advance(that, 1);
  ck_assert_int_eq(0x2, events_accepted);
  praef_metatransactor_advance(that, 1);
  ck_assert_int_eq(0x6, events_accepted);
  praef_metatransactor_advance(that, 10);
  ck_assert_int_eq(0x7, events_accepted);
}

deftest(wont_add_duplicate_event) {
  with_basic_events();
  ck_assert(praef_metatransactor_add_event(that, 1, bevent(1, 0)));
  ck_assert(!praef_metatransactor_add_event(that, 1, bevent(1, 0)));
}

deftest(wont_add_event_for_nonexistent_node) {
  with_basic_events();
  ck_assert(!praef_metatransactor_add_event(that, 99, bevent(1, 0)));
}

deftest(invalid_chmods_fail) {
  with_basic_events();
  ck_assert(!praef_metatransactor_chmod(that, 99, 1, NS_GRANT, 10));
  ck_assert(!praef_metatransactor_chmod(that, 1, 99, NS_GRANT, 10));
  ck_assert(!praef_metatransactor_chmod(that, 1, 1, ~0, 10));
  ck_assert(!praef_metatransactor_chmod(that, 1, 1, 0, 10));
}

static praef_event* return_null() { return NULL; }
deftest(chmod_handles_null_return_from_cxn) {
  cxn_node_count_delta = (praef_event*(*)(signed,praef_instant))return_null;
  ck_assert(!praef_metatransactor_chmod(that, 1, 1, NS_GRANT, 10));
}

deftest(duplicate_grant_has_no_effect) {
  with_basic_events();
  praef_metatransactor_add_node(that, 2);
  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 2);
  praef_metatransactor_add_event(that, 2, bevent(5, 0));
  praef_metatransactor_advance(that, 6);
  ck_assert_int_eq(1, events_accepted);
  ck_assert_int_eq(2, node_count);
  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 3);
  praef_metatransactor_advance(that, 0);
  ck_assert_int_eq(1, events_accepted);
  ck_assert_int_eq(2, node_count);
}

deftest(duplicate_earlier_grant_can_accept_new_events) {
  with_basic_events();
  praef_metatransactor_add_node(that, 2);
  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 3);
  praef_metatransactor_add_event(that, 2, bevent(2, 0));
  praef_metatransactor_advance(that, 4);
  ck_assert_int_eq(0, events_accepted);
  ck_assert_int_eq(2, node_count);
  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 1);
  praef_metatransactor_advance(that, 0);
  ck_assert_int_eq(1, events_accepted);
  ck_assert_int_eq(2, node_count);
}

deftest(duplicate_deny_has_no_effect) {
  with_basic_events();
  praef_metatransactor_add_node(that, 2);
  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 1);
  praef_metatransactor_chmod(that, 2, 1, NS_DENY, 1);
  praef_metatransactor_add_event(that, 2, bevent(4, 0));
  praef_metatransactor_advance(that, 5);
  ck_assert_int_eq(0, events_accepted);
  ck_assert_int_eq(1, node_count);
  praef_metatransactor_chmod(that, 2, 1, NS_DENY, 2);
  praef_metatransactor_advance(that, 0);
  ck_assert_int_eq(0, events_accepted);
  ck_assert_int_eq(1, node_count);
}

deftest(duplicate_earlier_deny_can_redact_events) {
  with_basic_events();
  praef_metatransactor_add_node(that, 2);
  praef_metatransactor_chmod(that, 2, 1, NS_GRANT, 2);
  praef_metatransactor_chmod(that, 2, 1, NS_DENY, 4);
  praef_metatransactor_add_event(that, 2, bevent(3, 0));
  praef_metatransactor_advance(that, 5);
  ck_assert_int_eq(1, events_accepted);
  ck_assert_int_eq(1, node_count);
  praef_metatransactor_chmod(that, 2, 1, NS_DENY, 1);
  praef_metatransactor_advance(that, 0);
  ck_assert_int_eq(0, events_accepted);
  ck_assert_int_eq(1, node_count);
}
