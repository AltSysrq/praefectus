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

#include <SDL.h>
#include <libpraefectus/virtual-bus.h>

#include "bsd.h"

#include "../alloc.h"
#include "../common.h"
#include "../game-state.h"
#include "../global-config.h"
#include "../graphics/canvas.h"
#include "../graphics/console.h"
#include "../graphics/crt.h"
#include "context.h"
#include "object.h"

#define NUM_STARS 4096

typedef struct {
  game_state self;
  game_state* parent;
  game_context* context;
  int is_alive;

  struct {
    int up, down, left, right;
  } pressed;

  struct {
    game_object_coord x, y;
  } stars[NUM_STARS];

  praef_virtual_network* virtual_network;
} gameplay_state;

static game_object* gameplay_state_get_self(gameplay_state*);
static game_state* gameplay_update(gameplay_state*, unsigned);
static void gameplay_draw(gameplay_state*, canvas*, crt_colour*);
static void gameplay_key(gameplay_state*, SDL_KeyboardEvent*);
static void gameplay_mbutton(gameplay_state*, SDL_MouseButtonEvent*);
static void gameplay_mmotion(gameplay_state*, SDL_MouseMotionEvent*);
static void gameplay_txtin(gameplay_state*, SDL_TextInputEvent*);
static void gameplay_state_delete(gameplay_state*);

static gameplay_state* gameplay_state_ctor(void) {
  gameplay_state* this = zxmalloc(sizeof(gameplay_state));
  unsigned i;

  this->self.update = (game_state_update_t)gameplay_update;
  this->self.draw = (game_state_draw_t)gameplay_draw;
  this->self.key = (game_state_key_t)gameplay_key;
  this->self.mbutton = (game_state_mbutton_t)gameplay_mbutton;
  this->self.mmotion = (game_state_mmotion_t)gameplay_mmotion;
  this->self.txtin = (game_state_txtin_t)gameplay_txtin;
  this->is_alive = 1;
  for (i = 0; i < NUM_STARS; ++i) {
    /* Two calls to cope with MSVCR which returns 15-bit integers */
    this->stars[i].x = rand() ^ (rand() << 15);
    this->stars[i].y = rand() ^ (rand() << 15);
  }

  return this;
}

game_state* gameplay_state_new(game_context* context,
                               game_state* parent) {
  gameplay_state* this = gameplay_state_ctor();

  this->parent = parent;
  this->context = context;
  this->virtual_network = NULL;

  return (game_state*)this;
}

game_state* gameplay_state_test(void) {
  gameplay_state* this;
  praef_virtual_network* vnet;
  praef_virtual_bus* vbus;
  game_context* context;

  vnet = praef_virtual_network_new();
  if (!vnet) errx(EX_UNAVAILABLE, "out of memory");
  vbus = praef_virtual_network_create_node(vnet);
  if (!vbus) errx(EX_UNAVAILABLE, "out of memory");

  context = xmalloc(sizeof(game_context));
  game_context_init(context, praef_virtual_bus_mb(vbus),
                    praef_virtual_bus_address(vbus));

  this = gameplay_state_ctor();
  this->context = context;
  this->parent = NULL;
  this->virtual_network = vnet;

  praef_system_bootstrap(context->sys);

  return (game_state*)this;
}

static void gameplay_state_delete(gameplay_state* this) {
  if (this->virtual_network)
    praef_virtual_network_delete(this->virtual_network);

  free(this);
}

static game_object* gameplay_state_get_self(gameplay_state* this) {
  praef_object_id id;

  id = praef_system_get_local_id(this->context->sys);
  if (id)
    return (game_object*)praef_context_get_object(
      this->context->state.context, id);
  else
    return NULL;
}

static game_state* gameplay_update(gameplay_state* this, unsigned et) {
  game_state* parent;
  game_object* self;

  if (this->virtual_network)
    praef_virtual_network_advance(this->virtual_network, et);

  if ((self = gameplay_state_get_self(this)))
    game_object_send_events(self, this->context->sys);

  game_context_update(this->context, et);

  if (praef_ss_ok != this->context->status)
    /* TODO: Better error reporting */
    this->is_alive = 0;

  if (this->is_alive) {
    return (game_state*)this;
  } else {
    parent = this->parent;
    gameplay_state_delete(this);
    return parent;
  }
}

static void gameplay_draw(gameplay_state* this, canvas* dst,
                          crt_colour* palette) {
  game_object* obj, * self;
  game_object_coord cx, cy;
  game_object_scoord sx, sy;
  game_object_core_state core;
  game_object_proj_state proj[MAX_PROJECTILES];
  unsigned i;

  crt_default_palette(palette);
  memset(dst->data, 0, dst->pitch * dst->h * sizeof(canvas_pixel));

  if ((self = gameplay_state_get_self(this))) {
    if (game_object_current_state(&core, proj, self)) {
      cx = core.x - dst->w*GOC_PIXEL_SIZE/2;
      cy = core.y - dst->h*GOC_PIXEL_SIZE/2;

      for (i = 0; i < NUM_STARS; ++i) {
        sx = this->stars[i].x - cx;
        sy = this->stars[i].y - cy;
        sx /= GOC_PIXEL_SIZE;
        sy /= GOC_PIXEL_SIZE;

        if (sx >= 0 && sx < dst->pitch &&
            sy >= 0 && sy < dst->h)
          dst->data[canvas_off(dst, sx, sy)] = CP_GREY + CP_SIZE/2;
      }

      SLIST_FOREACH(obj, &this->context->objects, next)
        game_object_draw(dst, obj, cx, cy);
    }
  }
}

static void gameplay_key(gameplay_state* this, SDL_KeyboardEvent* evt) {
  game_object* self = gameplay_state_get_self(this);
  BoundKey_t lsym = evt->keysym.sym;
  int down = (SDL_KEYDOWN == evt->type);

  if (SDLK_ESCAPE == evt->keysym.sym) {
    this->is_alive &= SDL_KEYDOWN != evt->type;
  } else if (global_config.controls.up == lsym) {
    this->pressed.up = down;
  } else if (global_config.controls.down == lsym) {
    this->pressed.down = down;
  } else if (global_config.controls.left == lsym) {
    this->pressed.left = down;
  } else if (global_config.controls.right == lsym) {
    this->pressed.right = down;
  }

  if (self) {
    self->want_xvel = this->pressed.right - this->pressed.left;
    self->want_yvel = this->pressed.down - this->pressed.up;
  }
}

static void gameplay_mmotion(gameplay_state* this, SDL_MouseMotionEvent* evt) {
  /* TODO */
}

static void gameplay_mbutton(gameplay_state* this, SDL_MouseButtonEvent* evt) {
  /* TODO */
}

static void gameplay_txtin(gameplay_state* this, SDL_TextInputEvent* txtin) {
  /* TODO */
}
