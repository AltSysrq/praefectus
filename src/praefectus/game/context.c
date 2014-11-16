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

#include "context.h"
#include "object.h"

void game_context_init(game_context* this) {
  SLIST_INIT(&this->objects);
}

void game_context_destroy(game_context* this) {
  game_object* obj, * tmp;

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
