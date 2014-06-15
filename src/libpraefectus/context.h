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
#ifndef LIBPRAEFECTUS_CONTEXT_H_
#define LIBPRAEFECTUS_CONTEXT_H_

#include "common.h"
#include "object.h"
#include "event.h"

/**
 * Opaque type for a single praefectus context. A context maintains a chain of
 * events and knowledge of the set of objects in existence, as well as the
 * current instant.
 */
typedef struct praef_context_s praef_context;

/**
 * Creates and returns a new, empty context. The empty context has an instant
 * of zero, no objects, and one event identified by the (0,0,0) triple.
 *
 * Returns NULL if allocating memory fails.
 */
praef_context* praef_context_new(void);
/**
 * Frees the given context and all the events it contains.
 */
void praef_context_delete(praef_context*);

/**
 * Adds the given object to the praefectus context. The id field of the object
 * MUST be set appropriately, and the object MUST be able to rewind to a point
 * in time before its creation.
 *
 * The object given will immediately be rewound to the current internal instant
 * of the context, which is not necessarily the current actual instant.
 *
 * If an object with the same id as the given object has already been added,
 * that object is returned and the context is not modified. If the given object
 * has id 0, the context is not modified and the input object is
 * returned. Otherwise, NULL is returned.
 */
praef_object* praef_context_add_object(praef_context*, praef_object*);
/**
 * Adds an event to the given context. The event may be any amount in the
 * future or past. If the object is in the past, all objects may be rewound to
 * that instant (if they have not already been rewound further), so
 * applications will generally want to restrict how far into the past they are
 * willing to allow new events to be.
 *
 * After this call, the memory used by the input event is under the control of
 * the context, regardless of whether the call succeeds. Events are freed by
 * invoking their free method.
 *
 * If the input event shares the id of another event, it is destroyed
 * immediately and the conflicting event is returned. If there is no object
 * corresponding to the event, it is destroyed immediately and the (0,0,0)
 * event is returned. Otherwise, NULL is returned, indicating success.
 *
 * After this call, objects are not considered to be in a consistent state
 * until the next call to praef_context_advance().
 */
praef_event* praef_context_add_event(praef_context*, praef_event*);
/**
 * Removes the event identified by the given (object,instant,serial_number)
 * triple from the context and destroys it. If the event is in the past, all
 * objects may be rewound to the instant of that event (if they have not
 * already been rewound further), so applications will generally want to
 * restrict how far into the past they are willing to redact events.
 *
 * Returns 1 if an event was removed, or 0 if there was no such event (or it
 * was the (0,0,0) event).
 *
 * After this call, objects are not considered to be in a consistent state
 * until the next call to praef_context_advance().
 */
int praef_context_redact_event(praef_context*,
                               praef_object_id, praef_instant,
                               praef_event_serial_number);

/**
 * Advances the given context delta_t steps forward in time. The given userdata
 * is passed to all callbacks that take a userdata argument. After this call,
 * all objects in the context are considered to be in a consistent state
 * corresponding to the instant retrievable from praef_context_now().
 *
 * Calling this with delta_t=0 is meaningful, and ensures all objects are
 * consistent without advancing time.
 */
void praef_context_advance(praef_context*, unsigned delta_t, praef_userdata);
/**
 * Returns the current logical instant of the given context. Note that between
 * event mutation calls and praef_context_advance(), objects might be in a
 * state corresponding to an instant before the value returned.
 */
praef_instant praef_context_now(const praef_context*);

/**
 * Returns the latest event in the context whose instant is greater than or
 * equal to the given instant; the `sebsequent` field on each event can be used
 * to traverse events according to application order.
 *
 * Returns NULL if no events exist beyond the given instant.
 */
const praef_event* praef_context_first_event_after(const praef_context*, praef_instant);

#endif /* LIBPRAEFECTUS_CONTEXT_H_ */
