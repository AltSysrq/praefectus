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

#include "../common.h"
#include "../asn1/GameEvent.h"
#include "object.h"
#include "event.h"
#include "context.h"

static unsigned game_context_optimistic_events(
  praef_app*, const praef_event*);
static void game_context_create_node_object(
  praef_app*, praef_object_id);
static praef_event* game_context_decode_event(
  praef_app*, praef_instant, praef_object_id,
  praef_event_serial_number, const void*, size_t);

void game_context_init(game_context* this,
                       praef_message_bus* bus,
                       const PraefNetworkIdentifierPair_t* netid) {
  SLIST_INIT(&this->objects);

  if (!praef_std_state_init(&this->state))
    errx(EX_UNAVAILABLE, "out of memory");

  this->app = praef_stdsys_new(&this->state);
  if (!this->app)
    errx(EX_UNAVAILABLE, "out of memory");

  praef_stdsys_set_userdata(this->app, this);
  praef_stdsys_optimistic_events(this->app, game_context_optimistic_events);
  this->app->decode_event = game_context_decode_event;
  this->app->create_node_object = game_context_create_node_object;

  this->sys = praef_system_new(this->app, bus, netid,
                               SECOND/4, praef_sp_lax,
                               praef_siv_4only,
                               netid->internet?
                               praef_snl_global :
                               praef_snl_local,
                               512);
  if (!this->sys)
    errx(EX_UNAVAILABLE, "out of memory");

  praef_stdsys_set_system(this->app, this->sys);
}

void game_context_destroy(game_context* this) {
  game_object* obj, * tmp;

  praef_system_delete(this->sys);
  praef_stdsys_delete(this->app);
  praef_std_state_cleanup(&this->state);

  SLIST_FOREACH_SAFE(obj, &this->objects, next, tmp)
    game_object_delete(obj);
}

void game_context_add_object(game_context* this, game_object* obj) {
  game_object* after, * next;

  if (SLIST_EMPTY(&this->objects) ||
      obj->self.id < SLIST_FIRST(&this->objects)->self.id) {
    SLIST_INSERT_HEAD(&this->objects, obj, next);
  } else {
    for (after = SLIST_FIRST(&this->objects),
           next = SLIST_NEXT(after, next);
         next && next->self.id < obj->self.id;
         after = next,
           next = SLIST_NEXT(after, next));
    SLIST_INSERT_AFTER(after, obj, next);
  }
}

static unsigned game_context_optimistic_events(praef_app* app,
                                               const praef_event* evt) {
  return SECOND/4;
}

static void game_context_create_node_object(praef_app* app,
                                            praef_object_id id) {
  game_context* this = praef_stdsys_userdata(app);
  game_object* obj;

  obj = game_object_new(this, id);
  game_context_add_object(this, obj);
  praef_context_add_object(this->state.context, (praef_object*)obj);
}

static praef_event* game_context_decode_event(
  praef_app* app, praef_instant instant, praef_object_id object,
  praef_event_serial_number serno,
  const void* data, size_t sz
) {
  game_event* evt;
  GameEvent_t edata, * edata_ptr = &edata;
  asn_dec_rval_t decode_result;

  memset(&edata, 0, sizeof(edata));
  decode_result = uper_decode_complete(
    NULL, &asn_DEF_GameEvent, (void**)&edata_ptr,
    data, sz);
  if (RC_OK != decode_result.code)
    evt = NULL;
  else
    evt = game_event_new(&edata, instant, object, serno);

  (*asn_DEF_GameEvent.free_struct)(&asn_DEF_GameEvent, &edata, 1);
  return (praef_event*)evt;
}

void game_context_update(game_context* this, unsigned et) {
  this->status = praef_system_advance(this->sys, et);
}
