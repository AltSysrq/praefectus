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
#include <string.h>

#include <libpraefectus/object.h>

#include "bsd.h"
#include "../common.h"
#include "../alloc.h"
#include "../global-config.h"
#include "../graphics/console.h"
#include "../asn1/GameEvent.h"
#include "context.h"
#include "object.h"

#define HEARTBEAT_INTERVAL (1*SECOND)
#define EXPIREY_INTERVAL (2*SECOND)
#define PROJECTILE_LIFETIME (1*SECOND)
#define OBJECT_SPEED (GOC_PIXEL_SIZE*64 / SECOND)
#define PROJECTILE_SPEED_128 (1)
#define OBJECT_SIZE 16

static void game_object_put_event(praef_system*, const GameEvent_t*);
static void game_object_step(game_object*, praef_userdata);
static void game_object_rewind(game_object*, praef_instant);
static void game_object_remove_last_state(game_object*);

game_object* game_object_new(game_context* context,
                             praef_object_id id) {
  game_object* this;

  this = zxmalloc(sizeof(game_object));
  this->self.id = id;
  this->self.step = (praef_object_step_t)game_object_step;
  this->self.rewind = (praef_object_rewind_t)game_object_rewind;
  this->context = context;
  this->core_states_cap = 256;
  this->core_states = xmalloc(sizeof(game_object_core_state) *
                              this->core_states_cap);
  this->proj_states_cap = 256;
  this->proj_states = xmalloc(sizeof(game_object_proj_state) *
                              this->proj_states_cap);

  return this;
}

void game_object_delete(game_object* this) {
  free(this->core_states);
  free(this->proj_states);
  free(this);
}

static void game_object_put_event(praef_system* sys,
                                  const GameEvent_t* evt) {
  unsigned char dst[64];
  asn_enc_rval_t encode_result;

  encode_result = uper_encode_to_buffer(
    &asn_DEF_GameEvent, (GameEvent_t*)evt, dst, sizeof(dst));
  if (-1 == encode_result.encoded)
    errx(EX_SOFTWARE, "Failed to PER-encode GameEvent");

  if (!praef_system_add_event(sys, dst, (encode_result.encoded + 7) / 8))
    errx(EX_SOFTWARE, "Failed to add event to system");
}

static inline signed sign(signed s) {
  if (s < 0) return -1;
  if (s > 0) return +1;
  return 0;
}

void game_object_send_events(game_object* this, praef_system* sys) {
  GameEvent_t evt;
  const game_object_core_state* last_state;

  memset(&evt, 0, sizeof(evt));
  /* Only valid if 0 != this->core_num_states */
  last_state = this->core_states + this->core_num_states - 1;

  if (0 == this->core_num_states) {
    evt.present = GameEvent_PR_initialise;
    memcpy(this->screen_name, global_config.screenname.buf,
           global_config.screenname.size);
    this->screen_name[global_config.screenname.size] = 0;
    evt.choice.initialise.screenname.buf = (unsigned char*)this->screen_name;
    evt.choice.initialise.screenname.size = strlen(this->screen_name);
    evt.choice.initialise.xpos = this->self.id & 0xFFFF;
    evt.choice.initialise.ypos = (this->self.id >> 16) & 0xFFFF;
  } else if (sign(this->want_xvel) != (signed)last_state->xvel - 1 ||
             sign(this->want_yvel) != (signed)last_state->yvel - 1) {
    evt.present = GameEvent_PR_setcontrol;
    evt.choice.setcontrol.xvel = sign(this->want_xvel);
    evt.choice.setcontrol.yvel = sign(this->want_yvel);
  } else if (this->now - last_state->instant >= HEARTBEAT_INTERVAL) {
    evt.present = GameEvent_PR_heartbeat;
  } else {
    /* No events to send */
    return;
  }

  game_object_put_event(sys, &evt);
}

void game_object_send_fire_one(const game_object* this, praef_system* sys,
                               signed short xo, signed short yo) {
  GameEvent_t evt;

  if (!xo && !yo) return;

  memset(&evt, 0, sizeof(evt));
  evt.present = GameEvent_PR_fire;
  evt.choice.fire.xoff = xo;
  evt.choice.fire.yoff = yo;
  game_object_put_event(sys, &evt);
}

void game_object_send_fire_all(const game_object* this, praef_system* sys,
                               signed short xo, signed short yo) {
  unsigned i;

  for (i = 0; i < MAX_PROJECTILES; ++i)
    game_object_send_fire_one(this, sys,
                              xo + (rand() & 0x1F) - 0x0F,
                              yo + (rand() & 0x1F) - 0x0F);
}

void game_object_draw(canvas* dst, const game_object* this,
                      game_object_coord cx,
                      game_object_coord cy) {
  game_object_scoord sx, sy;
  game_object_core_state core;
  game_object_proj_state proj[MAX_PROJECTILES];
  signed xo, yo;
  unsigned i;
  canvas_pixel colour;

  if (!game_object_current_state(&core, proj, this))
    return;

  sx = core.x - cx;
  sy = core.y - cy;
  sx /= GOC_PIXEL_SIZE;
  sy /= GOC_PIXEL_SIZE;

  /* TODO, maybe make graphics a bit more interesting. */
  colour = this->self.id * CP_SIZE - 1;
  for (yo = -OBJECT_SIZE/2; yo < OBJECT_SIZE/2; ++yo)
    if (sy + yo >= 0 && sy + yo < dst->h)
      for (xo = -OBJECT_SIZE/2; xo < OBJECT_SIZE/2; ++xo)
        if (sx + xo >= 0 && sx + xo < dst->w)
          dst->data[canvas_off(dst, sx+xo, sy+yo)] = colour;

  for (i = 0; i < core.nproj; ++i) {
    sx = proj[i].x - cx;
    sy = proj[i].y - cy;
    sx /= GOC_PIXEL_SIZE;
    sy /= GOC_PIXEL_SIZE;

    for (yo = -1; yo <= +1; ++yo)
      for (xo = -1; xo <= +1; ++xo)
        canvas_put(dst, sx+xo, sy+yo, colour);
  }
}

int game_object_current_state(game_object_core_state* core,
                              game_object_proj_state proj[MAX_PROJECTILES],
                              const game_object* this) {
  unsigned lt, et, i, j;

  if (0 == this->core_num_states) return 0;

  *core = this->core_states[this->core_num_states-1];
  et = this->now - core->instant;
  if (et >= EXPIREY_INTERVAL) return 0;

  core->x += et * ((signed)core->xvel - 1) * OBJECT_SPEED;
  core->y += et * ((signed)core->yvel - 1) * OBJECT_SPEED;
  core->instant = this->now;

  for (i = j = 0; i < core->nproj; ++i) {
    lt = this->now - this->proj_states[this->proj_num_states-1 - i].created_at;
    et = this->now - this->proj_states[this->proj_num_states-1 - i].instant;
    if (lt < PROJECTILE_LIFETIME) {
      proj[j] = this->proj_states[this->proj_num_states-1 - i];
      proj[j].x += proj[j].vx * et * PROJECTILE_SPEED_128;
      proj[j].y += proj[j].vy * et * PROJECTILE_SPEED_128;
      proj[j].instant = this->now;
      ++j;
    }
  }

  core->nproj = j;
  return 1;
}

void game_object_put_state(game_object* this,
                           const game_object_core_state* core,
                           const game_object_proj_state proj[MAX_PROJECTILES]) {
  /* If the new state is at the same instant as the most recent state, replace
   * that one instead of using a new slot.
   */
  if (this->core_num_states &&
      core->instant == this->core_states[this->core_num_states-1].instant)
    game_object_remove_last_state(this);

  if (this->core_num_states == this->core_states_cap) {
    this->core_states_cap *= 2;
    this->core_states = xrealloc(
      this->core_states,
      sizeof(game_object_core_state) * this->core_states_cap);
  }

  if (this->proj_num_states + core->nproj > this->proj_states_cap) {
    this->proj_states_cap *= 2;
    this->proj_states = xrealloc(
      this->proj_states,
      sizeof(game_object_proj_state) * this->proj_states_cap);
  }

  this->core_states[this->core_num_states++] = *core;
  memcpy(this->proj_states + this->proj_num_states, proj,
         core->nproj * sizeof(game_object_proj_state));
  this->proj_num_states += core->nproj;
}

void game_object_step(game_object* this, praef_userdata _) {
  game_object_core_state this_core, other_core;
  game_object_proj_state this_proj[MAX_PROJECTILES],
                         other_proj[MAX_PROJECTILES];
  game_object* other;
  unsigned i;
  game_object_scoord dx, dy;

  ++this->now;

  if (!game_object_current_state(&this_core, this_proj, this))
    return;

  if (!this_core.nproj) return;

  /* Check for collisions between this object's projectiles and other objects.
   */
  SLIST_FOREACH(other, &this->context->objects, next) {
    if (this == other) continue;
    if (!game_object_current_state(&other_core, other_proj, other))
      continue;

    for (i = 0; i < this_core.nproj; ++i) {
      dx = this_proj[i].x - other_core.x;
      dy = this_proj[i].y - other_core.y;

      if (abs(dx) <= OBJECT_SIZE*GOC_PIXEL_SIZE &&
          abs(dy) <= OBJECT_SIZE*GOC_PIXEL_SIZE) {
        if (!other_core.hp) {
          /* Other player killed */
          other_core.hp = ~0;
          other_core.x += 0x8000;
          other_core.y += 0x8000;

          ++this_core.score;
        } else {
          --other_core.hp;
        }

        game_object_put_state(other, &other_core, other_proj);

        /* In either case, the projectile is consumed */
        memmove(this_proj + i, this_proj + i + 1,
                sizeof(game_object_proj_state) * (this_core.nproj-i-1));
        --i;
        game_object_put_state(this, &this_core, this_proj);
      }
    }
  }
}

void game_object_rewind(game_object* this, praef_instant instant) {
  this->now = instant;

  while (this->core_num_states &&
         instant >= this->core_states[this->core_num_states-1].instant)
    game_object_remove_last_state(this);
}

static void game_object_remove_last_state(game_object* this) {
  this->proj_num_states -= this->core_states[this->core_num_states-1].nproj;
  --this->core_num_states;
}
