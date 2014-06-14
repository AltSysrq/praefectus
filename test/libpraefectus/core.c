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

defsuite(libpraefectus_core);

static praef_context* context;

defsetup {
  context = praef_context_new();
}

defteardown {
  praef_context_delete(context);
}

deftest(object_ids_compare_as_defined) {
  praef_object a, b;

  a.id = 0;
  b.id = 1;
  ck_assert_int_eq(-1, praef_compare_object_id(&a, &b));
  ck_assert_int_eq(+1, praef_compare_object_id(&b, &a));
  b.id = 0;
  ck_assert_int_eq( 0, praef_compare_object_id(&a, &b));
}

deftest(events_compare_as_defined) {
  praef_event a, b;

  a.instant = 0;
  a.object = 9;
  a.serial_number = 9;
  b.instant = 1;
  b.object = 0;
  b.serial_number = 0;
  ck_assert_int_eq(-1, praef_compare_event_sequence(&a, &b));
  ck_assert_int_eq(+1, praef_compare_event_sequence(&b, &a));

  a.instant = 0;
  a.object = 0;
  a.serial_number = 9;
  b.instant = 0;
  b.object = 1;
  b.serial_number = 0;
  ck_assert_int_eq(-1, praef_compare_event_sequence(&a, &b));
  ck_assert_int_eq(+1, praef_compare_event_sequence(&b, &a));

  a.instant = 0;
  a.object = 0;
  a.serial_number = 0;
  b.instant = 0;
  b.object = 0;
  b.serial_number = 1;
  ck_assert_int_eq(-1, praef_compare_event_sequence(&a, &b));
  ck_assert_int_eq(+1, praef_compare_event_sequence(&b, &a));

  b.serial_number = 0;
  ck_assert_int_eq( 0, praef_compare_event_sequence(&a, &b));
}

static void noop() { }

deftest(wont_add_object_with_id_zero) {
  praef_object obj;
  obj.id = PRAEF_NULL_OBJECT_ID;
  obj.rewind = (praef_object_rewind_t)noop;

  ck_assert_ptr_eq(&obj, praef_context_add_object(context, &obj));
}

deftest(adds_object_exactly_once) {
  praef_object obj, other;
  other.id = obj.id = 42;
  other.rewind = obj.rewind = (praef_object_rewind_t)noop;

  ck_assert_ptr_eq(NULL, praef_context_add_object(context, &obj));
  ck_assert_ptr_eq(&obj, praef_context_add_object(context, &other));
  ck_assert_ptr_eq(&obj, praef_context_add_object(context, &other));
}

deftest(adding_object_rewinds_to_current_time) {
  praef_instant rewound_to = 0;
  praef_object obj = {
    .id = 42,
    .rewind = lambdav((praef_object* this, praef_instant when),
                      ck_assert_ptr_eq(&obj, this);
                      rewound_to = when)
  };

  praef_context_advance(context, 10, NULL);
  praef_context_add_object(context, &obj);
  ck_assert_int_eq(rewound_to, 10);
}

deftest(cannot_add_event_for_nonexistent_object) {
  int is_freed = 0;
  praef_event evt = {
    .object = 42,
    .instant = 0,
    .serial_number = 0,
    .free = lambdav((void* this),
                    ck_assert_ptr_eq(&evt, this);
                    ck_assert(!is_freed);
                    is_freed = 1),
  };

  ck_assert(praef_context_add_event(context, &evt));
  ck_assert(is_freed);
}

deftest(event_is_freed_with_context) {
  praef_context* context;
  int is_freed = 0;
  praef_event evt = {
    .object = 42,
    .instant = 0,
    .serial_number = 0,
    .free = lambdav((void* this),
                    ck_assert_ptr_eq(&evt, this);
                    ck_assert(!is_freed);
                    is_freed = 1),
  };
  praef_object obj = {
    .id = 42,
    .rewind = (praef_object_rewind_t)noop,
  };

  context = praef_context_new();
  praef_context_add_object(context, &obj);
  ck_assert(!praef_context_add_event(context, &evt));
  ck_assert(!is_freed);
  praef_context_delete(context);

  ck_assert(is_freed);
}

deftest(cannot_add_dupe_event) {
  int is_freed = 0;
  static praef_event orig = {
    .object = 42,
    .instant = 0,
    .serial_number = 0,
    .free = (praef_free_t)noop,
  };
  praef_event dupe = {
    .object = 42,
    .instant = 0,
    .serial_number = 0,
    .free = lambdav((void* this),
                    ck_assert_ptr_eq(&dupe, this);
                    ck_assert(!is_freed);
                    is_freed = 1),
  };
  praef_object obj = {
    .id = 42,
    .rewind = (praef_object_rewind_t)noop,
  };

  praef_context_add_object(context, &obj);
  ck_assert(!praef_context_add_event(context, &orig));
  ck_assert_ptr_eq(&orig, praef_context_add_event(context, &dupe));
  ck_assert(is_freed);
}

deftest(event_insertion_does_not_rewind_to_present) {
  static praef_event evt = {
    .object = 42,
    .instant = 10,
    .serial_number = 0,
    .free = (praef_free_t)noop,
  };
  praef_object obj = {
    .id = 42,
    .rewind = (praef_object_rewind_t)noop,
  };

  praef_context_advance(context, 10, NULL);
  praef_context_add_object(context, &obj);
  obj.rewind = NULL;
  ck_assert(!praef_context_add_event(context, &evt));
}

deftest(event_insertion_does_not_rewind_to_future) {
  static praef_event evt = {
    .object = 42,
    .instant = 20,
    .serial_number = 0,
    .free = (praef_free_t)noop,
  };
  praef_object obj = {
    .id = 42,
    .rewind = (praef_object_rewind_t)noop,
  };

  praef_context_advance(context, 10, NULL);
  praef_context_add_object(context, &obj);
  obj.rewind = NULL;
  ck_assert(!praef_context_add_event(context, &evt));
}

deftest(event_insertion_rewinds_to_past) {
  praef_instant actual_now = 0;
  static praef_event evt = {
    .object = 42,
    .instant = 5,
    .serial_number = 0,
    .free = (praef_free_t)noop,
  };
  praef_object obj = {
    .id = 42,
    .rewind = lambdav((praef_object* this, praef_instant now),
                      ck_assert_ptr_eq(&obj, this);
                      actual_now = now),
  };

  praef_context_advance(context, 10, NULL);
  praef_context_add_object(context, &obj);
  ck_assert_int_eq(10, actual_now);
  ck_assert(!praef_context_add_event(context, &evt));
  ck_assert_int_eq(5, actual_now);
}

deftest(cannot_redact_null_event) {
  ck_assert(!praef_context_redact_event(context, 0, 0, 0));
}

deftest(cannot_redact_nonexistent_event) {
  static praef_event evt = {
    .object = 1,
    .instant = 2,
    .serial_number = 0,
    .free = (praef_free_t)noop
  };
  praef_object obj = {
    .id = 1,
    .rewind = (praef_object_rewind_t)noop,
  };

  praef_context_add_object(context, &obj);
  ck_assert(!praef_context_add_event(context, &evt));
  ck_assert(!praef_context_redact_event(context, 1, 2, 3));
}

deftest(can_redact_existing_event_once) {
  int is_freed = 0;
  praef_event evt = {
    .object = 42,
    .instant = 0,
    .serial_number = 0,
    .free = lambdav((void* this),
                    ck_assert_ptr_eq(&evt, this);
                    ck_assert(!is_freed);
                    is_freed = 1),
  };
  praef_object obj = {
    .id = 42,
    .rewind = (praef_object_rewind_t)noop,
  };

  praef_context_add_object(context, &obj);
  ck_assert(!praef_context_add_event(context, &evt));
  ck_assert(!is_freed);
  ck_assert(praef_context_redact_event(context, 42, 0, 0));
  ck_assert(is_freed);
  ck_assert(!praef_context_redact_event(context, 42, 0, 0));
  is_freed = 0;
  ck_assert(!praef_context_add_event(context, &evt));
  ck_assert(!is_freed);
  ck_assert(praef_context_redact_event(context, 42, 0, 0));
  ck_assert(is_freed);
  ck_assert(!praef_context_redact_event(context, 42, 0, 0));
}

deftest(event_redaction_does_not_rewind_to_present) {
  praef_event evt = {
    .object = 42,
    .instant = 10,
    .serial_number = 0,
    .free = (praef_free_t)noop,
  };
  praef_object obj = {
    .id = 42,
    .rewind = (praef_object_rewind_t)noop,
  };

  praef_context_advance(context, 10, NULL);
  praef_context_add_object(context, &obj);
  obj.rewind = NULL;
  praef_context_add_event(context, &evt);
  ck_assert(praef_context_redact_event(context, 42, 10, 0));
}

deftest(event_redaction_does_not_rewind_to_future) {
  praef_event evt = {
    .object = 42,
    .instant = 10,
    .serial_number = 0,
    .free = (praef_free_t)noop,
  };
  praef_object obj = {
    .id = 42,
    .rewind = (praef_object_rewind_t)noop,
  };

  praef_context_advance(context, 10, NULL);
  praef_context_add_object(context, &obj);
  obj.rewind = NULL;
  praef_context_add_event(context, &evt);
  /* Mutating the event like this is technically illegal, but it's safe given
   * the implementation and the fact that the tree remains valid as long as we
   * don't mutate the event to (0,0,0) since it is the only event in the tree
   * besides the null event.
   */
  evt.instant = 20;
  ck_assert(praef_context_redact_event(context, 42, 20, 0));
}

deftest(event_redaction_rewinds_to_past) {
  praef_instant actual_now = 0;
  praef_event evt = {
    .object = 42,
    .instant = 10,
    .serial_number = 0,
    .free = (praef_free_t)noop,
  };
  praef_object obj = {
    .id = 42,
    .rewind = lambdav((praef_object* this, praef_instant when),
                      ck_assert_ptr_eq(&obj, this);
                      actual_now = when),
  };

  praef_context_advance(context, 10, NULL);
  praef_context_add_object(context, &obj);
  ck_assert_int_eq(10, actual_now);
  praef_context_add_event(context, &evt);
  /* Mutating the event like this is technically illegal, but it's safe given
   * the implementation and the fact that the tree remains valid as long as we
   * don't mutate the event to (0,0,0) since it is the only event in the tree
   * besides the null event.
   */
  evt.instant = 5;
  ck_assert(praef_context_redact_event(context, 42, 5, 0));
  ck_assert_int_eq(5, actual_now);
}
