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

#include "../alloc.h"
#include "object.h"
#include "event.h"

static void game_event_apply(game_object*, const game_event*,
                             praef_userdata);

static unsigned isqrt(unsigned n) {
  unsigned x0, x1 = n, delta;
  if (!n) return 0;

  do {
    x0 = x1;
    x1 = (x0 + n/x0) / 2;
    delta = x0 - x1;
  } while (delta && ~delta && delta-1);

  return x1;
}

game_event* game_event_new(const GameEvent_t* data,
                           praef_instant instant,
                           praef_object_id object,
                           praef_event_serial_number serno) {
  game_event* this = xmalloc(sizeof(game_event));

  this->self.instant = instant;
  this->self.object = object;
  this->self.serial_number = serno;
  this->self.apply = (praef_event_apply_t)game_event_apply;
  this->self.free = free;
  this->data = *data;
  if (GameEvent_PR_initialise == this->data.present) {
    memcpy(this->screenname_buf, this->data.choice.initialise.screenname.buf,
           this->data.choice.initialise.screenname.size);
    this->data.choice.initialise.screenname.buf = this->screenname_buf;
  }
  return this;
}

static void game_event_apply(game_object* obj,
                             const game_event* this,
                             praef_userdata _) {
  game_object_core_state core;
  game_object_proj_state proj[MAX_PROJECTILES];
  signed length;
  int is_alive;

  is_alive = game_object_current_state(&core, proj, obj);

  switch (this->data.present) {
  case GameEvent_PR_NOTHING: abort();

  case GameEvent_PR_initialise:
    if (is_alive) return;
    if (obj->core_num_states) return;

    memcpy(obj->screen_name, this->data.choice.initialise.screenname.buf,
           this->data.choice.initialise.screenname.size);
    obj->screen_name[this->data.choice.initialise.screenname.size] = 0;

    memset(&core, 0, sizeof(core));
    core.instant = obj->now;
    core.x = this->data.choice.initialise.xpos;
    core.y = this->data.choice.initialise.ypos;
    core.hp = ~0;
    core.xvel = 1;
    core.yvel = 1;
    break;

  case GameEvent_PR_heartbeat:
    if (!is_alive)
      return;
    else
      break;

  case GameEvent_PR_setcontrol:
    if (!is_alive) return;

    core.xvel = this->data.choice.setcontrol.xvel + 1;
    core.yvel = this->data.choice.setcontrol.yvel + 1;
    break;

  case GameEvent_PR_fire:
    if (!is_alive) return;
    if (!this->data.choice.fire.xoff &&
        !this->data.choice.fire.yoff)
      return;
    if (MAX_PROJECTILES == core.nproj)
      return;

    proj[core.nproj].created_at = core.instant;
    proj[core.nproj].instant = core.instant;
    proj[core.nproj].x = core.x;
    proj[core.nproj].y = core.y;
    length = isqrt(this->data.choice.fire.xoff *
                     this->data.choice.fire.xoff +
                   this->data.choice.fire.yoff *
                     this->data.choice.fire.yoff);
    proj[core.nproj].vx = this->data.choice.fire.xoff * 127 / length;
    proj[core.nproj].vy = this->data.choice.fire.yoff * 127 / length;
    ++core.nproj;
    break;
  }

  game_object_put_state(obj, &core, proj);
}
