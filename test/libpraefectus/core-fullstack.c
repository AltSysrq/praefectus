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
#include <libpraefectus/object.h>
#include <libpraefectus/event.h>
#include <libpraefectus/context.h>

/**
 * This suite is not a unit test, but rather exercises the entire core of
 * libpraefectus via simulation.
 *
 * A total of 1024 events are created, all applying against one object at
 * each instant between 0 and 1023, inclusive. The events are shuffled as to
 * when they will be introduced.
 *
 * At each of the 1024 iterations of the simulation, one of the events is
 * inserted, if it has not already been redacted, and the context is advanced
 * one step. On each even-numbered iteration, one of the even-numbered events
 * is redacted at random.
 *
 * Every time an event is applied to an object, it is asserted that no later
 * event has already been applied. Furthermore, when the test concludes, it is
 * verified that all odd-numbered events have been applied, and that no
 * even-numbered events have been applied.
 */
defsuite(libpraefectus_core_fullstack);

#define NUM_EVTS 1024

typedef struct {
  signed short latest_event;
  unsigned char events_applied[NUM_EVTS];
} object_state;

deftest(fullstack) {
  /* Some of these are static because they are too large to fit safely on the
   * stack, and we don't need to support any kind of thread-safety or anything
   * in the context of the tests.
   */
  praef_instant now = 0;
  /* The state[t] is the state of the object at the start of the instant
   * t. Therefore, at any time t, `now` actually points to t+1, since the
   * events will mutate it to put it into the state for the *next* step.
   */
  static object_state states[NUM_EVTS+2];
  static praef_event evts[NUM_EVTS];
  char inserted[NUM_EVTS] = { 0 }, redacted[NUM_EVTS] = { 0 };
  char freed[NUM_EVTS] = { 0 };
  unsigned i, r;

  praef_context* context;
  praef_object obj = {
    .id = 1,
    .rewind = lambdav((praef_object* this, praef_instant t),
                      now = t+1;
                      memcpy(states+now, states+now-1, sizeof(object_state))),
    .step = lambdav((praef_object* this, praef_userdata ud),
                    ++now;
                    ck_assert_int_lt(now, NUM_EVTS+2);
                    memcpy(states+now, states+now-1, sizeof(object_state))),
  };

  states[0].latest_event = -1;

  for (i = 0; i < NUM_EVTS; ++i) {
    evts[i].instant = i;
    evts[i].object = 1;
    evts[i].serial_number = i;
    evts[i].apply = lambdav((praef_object* tgt, const praef_event* this,
                             praef_userdata ud),
                            ck_assert_int_lt(states[now].latest_event,
                                             this->instant);
                            ck_assert(!states[now].events_applied[
                                        this->instant]);
                            states[now].latest_event = this->instant;
                            states[now].events_applied[this->instant] = 1);
    evts[i].free = (praef_free_t)lambdav((praef_event* this),
                                         ck_assert(!freed[this->instant]);
                                         freed[this->instant] = 1);
  }

  context = praef_context_new();
  praef_context_add_object(context, &obj);

  for (i = 0; i < NUM_EVTS; ++i) {
    do {
      r = rand() % NUM_EVTS;
    } while (inserted[r]);

    if (!redacted[r]) {
      praef_context_add_event(context, evts+r);
    } else {
      /* Since we never actually give the event to praefectus, just mark it as
       * freed immediately.
       */
      freed[r] = 1;
    }
    inserted[r] = 1;

    if (!(i&1)) {
      do {
        r = (rand() % NUM_EVTS) | 1;
      } while (redacted[r]);

      praef_context_redact_event(context, 1, r, r);
      redacted[r] = 1;
    }

    praef_context_advance(context, 1, NULL);
  }

  praef_context_delete(context);

  ck_assert_int_eq(NUM_EVTS, now-1);
  ck_assert_int_eq(NUM_EVTS-2, states[now].latest_event);
  for (i = 0; i < NUM_EVTS; ++i) {
    ck_assert_int_eq(i & 1, !states[now].events_applied[i]);
    ck_assert(freed[i]);
  }
}
