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
#ifndef LIBPRAEFECTUS_EVENT_H_
#define LIBPRAEFECTUS_EVENT_H_

#include "common.h"
#inlcude "object.h"

/**
 * The serial number type for an event.
 */
typedef unsigned praef_event_serial_number;
typedef struct praef_event_s praef_event;

/**
 * Function type for the apply method on an event. The given event is to mutate
 * the curent state of the given object as per the application-defined
 * semantics of the event and the object.
 */
typedef void (*praef_event_apply_t)(praef_object*, const praef_event*,
                                    praef_userdata);

/**
 * Events describe external stimuli which take effect on an object at a
 * specific time.
 *
 * The only primitive defined by an event is its apply method, which executes
 * the desired effect(s). Especially in applications that represent events as
 * direct user input, it is possible that information acquired out-of-order can
 * cause certain events to be nonsensical; however, there is no such thing as
 * an "invalid" event --- an event which is applied in a situation in which it
 * has no useful interpretation should simply have no effect.
 *
 * Events are uniquely (within a context) identified by their
 * (object,instant,serial_number) triple. This triple also defines the total
 * ordering of events; given to events A and B:
 * - If A's instant is lesser than B's instant, A precedes B
 * - If A's object is lesser than B's object, A precedes B
 * - If A's serial number is lesser than B's serial number, A precedes B
 */
struct praef_event_s {
  /**
   * The apply method for this event.
   */
  praef_event_apply_t apply;
  /**
   * The deallocator for this event.
   */
  praef_free_t free;

  /**
   * The id of the object to which this event will apply.
   */
  praef_object_id object;
  /**
   * The instant at which this event will apply.
   */
  praef_instant instant;
  /**
   * The serial number of this event. This is primarily a uniquifier, but also
   * specifies the ordering for multiple events against the same object in the
   * same instant.
   *
   * Conventionally, events for an object begin at zero and increase
   * monotonically over the course of the simulation. However, this is not
   * required; the only requirement is that no two events against the same
   * object share the same serial number.
   */
  praef_event_serial_number serial_number;

  /**
   * Splay tree entry used for ordering and identification of events.
   */
  SPLAY_ENTRY(praef_event_s) sequence;
  /**
   * Doubly-linked-list entry for traversing events, sorted ascending. This is
   * the preferred way to iterate over events in a linear fashion, as it does
   * not have the side-effect of pessimising the splay tree.
   */
  TAILQ_ENTRY(praef_event_s) subsequent;
};

/**
 * Compares the given elements for sequence, as per tree(3).
 */
int praef_compare_event_sequence(const praef_event*, const praef_event*);

SPLAY_PROTOTYPE(praef_event_sequence, praef_event_s, sequence,
                praef_compare_event_sequence);

#endif /* LIBPRAEFECTUS_EVENT_H_ */
