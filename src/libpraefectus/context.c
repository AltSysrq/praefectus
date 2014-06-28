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

#include <string.h>

#include "context.h"

struct praef_context_s {
  TAILQ_HEAD(, praef_event_s) events;
  struct praef_event_sequence event_sequence;
  struct praef_object_idmap objects;

  praef_event null_event;

  /* The current instant held by all objects. */
  praef_instant actual_now;
  /* The current instant that client code expects the context to have. */
  praef_instant logical_now;
};

static void noop_apply(praef_object* obj, const praef_event* this,
                       praef_userdata ignore) {
}

static void noop_free(void* _) { }

praef_context* praef_context_new(void) {
  praef_context* this;

  this = malloc(sizeof(praef_context));
  if (!this) return NULL;

  this->null_event.free = noop_free;
  this->null_event.apply = noop_apply;
  this->null_event.object = 0;
  this->null_event.instant = 0;
  this->null_event.serial_number = 0;

  this->actual_now = 0;
  this->logical_now = 0;

  TAILQ_INIT(&this->events);
  SPLAY_INIT(&this->event_sequence);
  RB_INIT(&this->objects);

  SPLAY_INSERT(praef_event_sequence, &this->event_sequence, &this->null_event);
  TAILQ_INSERT_HEAD(&this->events, &this->null_event, subsequent);

  return this;
}

void praef_context_delete(praef_context* this) {
  praef_event* evt, * tmp;

  TAILQ_FOREACH_SAFE(evt, &this->events, subsequent, tmp)
    (*evt->free)(evt);

  free(this);
}

praef_object* praef_context_add_object(praef_context* this, praef_object* obj) {
  praef_object* already_existing;

  if (!obj->id) return obj;

  already_existing = RB_INSERT(praef_object_idmap, &this->objects, obj);

  if (!already_existing)
    /* Inserted, need to inform the object of the actual now */
    (*obj->rewind)(obj, this->actual_now);

  return already_existing;
}

static void praef_context_roll_back(praef_context* this, praef_instant when) {
  praef_object* it;

  if (when < this->actual_now) {
    this->actual_now = when;
    RB_FOREACH(it, praef_object_idmap, &this->objects)
      (*it->rewind)(it, this->actual_now);
  }
}

praef_event* praef_context_add_event(praef_context* this, praef_event* evt) {
  praef_event* already_existing, * preceding, * next;
  praef_object obj;

  obj.id = evt->object;
  if (!RB_FIND(praef_object_idmap, &this->objects, &obj)) {
    (*evt->free)(evt);
    return &this->null_event;
  }

  already_existing = SPLAY_INSERT(
    praef_event_sequence, &this->event_sequence, evt);
  if (already_existing) {
    (*evt->free)(evt);
    return already_existing;
  }

  /* No conflict. Add to the sorted list.
   *
   * First, we must locate the event that immediately precedes this one. Since
   * this is a splay tree (meaning that the new event is now the root of the
   * tree), this will always be the right-most descendant of the new event's
   * left child.
   */
  preceding = SPLAY_LEFT(evt, sequence);
  while ((next = SPLAY_RIGHT(preceding, sequence)))
    preceding = next;
  TAILQ_INSERT_AFTER(&this->events, preceding, evt, subsequent);

  /* Inserted successfully. Roll back if needed. */
  praef_context_roll_back(this, evt->instant);

  return NULL;
}

int praef_context_redact_event(praef_context* this,
                               praef_object_id object,
                               praef_instant instant,
                               praef_event_serial_number serial_number) {
  praef_event example, * evt;

  /* Can't remove null event */
  if (!object && !instant && !serial_number)
    return 0;

  example.object = object;
  example.instant = instant;
  example.serial_number = serial_number;
  evt = SPLAY_FIND(praef_event_sequence, &this->event_sequence, &example);
  if (evt) {
    /* The event can be removed. Roll back if needed. */
    praef_context_roll_back(this, evt->instant);

    TAILQ_REMOVE(&this->events, evt, subsequent);
    SPLAY_REMOVE(praef_event_sequence, &this->event_sequence, evt);
    (*evt->free)(evt);
    return 1;
  } else {
    return 0;
  }
}

praef_instant praef_context_now(const praef_context* this) {
  return this->logical_now;
}

const praef_event* praef_context_first_event_after(const praef_context* this,
                                                   praef_instant when) {
  praef_event* evt, * next;

  next = SPLAY_ROOT(&this->event_sequence);
  do {
    evt = next;
    if (when > evt->instant) next = SPLAY_RIGHT(evt, sequence);
    else                     next = SPLAY_LEFT(evt, sequence);
  } while (next);

  /* The above loop will either terminate on the last event before the chosen
   * time, or the first event after it, depending on the structure of the
   * tree. Check for the former case and move one element forward.
   */
  if (evt->instant < when) evt = TAILQ_NEXT(evt, subsequent);

  return evt;
}

const praef_event* praef_context_get_event(
  const praef_context* this, praef_object_id object,
  praef_instant instant, praef_event_serial_number serial_number
) {
  praef_event example;
  example.object = object;
  example.instant = instant;
  example.serial_number = serial_number;

  return SPLAY_FIND(praef_event_sequence,
                    &((praef_context*)this)->event_sequence, &example);
}

praef_object* praef_context_get_object(const praef_context* this,
                                       praef_object_id id) {
  praef_object example;
  example.id = id;
  return RB_FIND(praef_object_idmap,
                 &((praef_context*)this)->objects, &example);
}

void praef_context_advance(praef_context* this, unsigned delta_t,
                           praef_userdata userdata) {
  praef_object* obj, obj_by_id;
  const praef_event* event_queue =
    praef_context_first_event_after(this, this->actual_now);

  this->logical_now += delta_t;

  while (this->actual_now != this->logical_now) {
    /* Apply all events that take effect this step */
    while (event_queue && event_queue->instant == this->actual_now) {
      obj_by_id.id = event_queue->object;
      (*event_queue->apply)(RB_FIND(praef_object_idmap, &this->objects,
                                    &obj_by_id),
                            event_queue, userdata);
      event_queue = TAILQ_NEXT(event_queue, subsequent);
    }

    /* Step all objects to the next instant */
    RB_FOREACH(obj, praef_object_idmap, &this->objects)
      (*obj->step)(obj, userdata);

    ++this->actual_now;
  }
}
