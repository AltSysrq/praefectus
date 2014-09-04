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

#include <SDL.h>

#include "../alloc.h"
#include "../graphics/canvas.h"
#include "../graphics/crt.h"
#include "../graphics/font.h"
#include "../graphics/fraktur.h"
#include "../game-state.h"
#include "test-state.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

typedef struct {
  game_state self;
  int has_rendered;
  int is_alive;
  signed time_till_step;
  compiled_font font;
} test_state;

static game_state* test_state_update(test_state*, unsigned);
static void test_state_draw(test_state*, canvas*, crt_colour*);
static void test_state_key(test_state*, SDL_KeyboardEvent*);

game_state* test_state_new(void) {
  test_state* this = xmalloc(sizeof(test_state));

  memset(this, 0, sizeof(test_state));
  this->self.update = (game_state_update_t)test_state_update;
  this->self.draw = (game_state_draw_t)test_state_draw;
  this->self.key = (game_state_key_t)test_state_key;
  this->is_alive = 1;
  this->has_rendered = 0;
  this->time_till_step = 0;
  font_compile(&this->font, &fraktur);
  return (game_state*)this;
}

void test_state_delete(game_state* this) {
  free(this);
}

static game_state* test_state_update(test_state* this, unsigned et) {
  this->time_till_step -= et;

  if (this->is_alive) {
    return (game_state*)this;
  } else {
    test_state_delete((game_state*)this);
    return NULL;
  }
}

static const crt_colour demon_palette[16] = {
  0x000000, /* 0 */
  0x020202,
  0x040404,
  0x060606,
  0x080808, /* 4 */
  0x101010,
  0x202020,
  0x3F3F3F,
  0x003F3F, /* 8 */
  0x00003F,
  0x3F003F,
  0x3F0000,
  0x3F3F00, /* 12 */
  0x003F00,
  0x001010,
  0x000004,
};

static void test_state_draw(test_state* this, canvas* dst,
                            crt_colour* palette) {
  /* For the purposes of testing, do a demon automaton directly in the
   * canvas. The particular palette used here is designed to produce large dark
   * regions.
   *
   * Note we only use the first 16 colours.
   */
  canvas_pixel prev[dst->pitch * dst->h];
  unsigned x, y;
  signed ox, oy;
  canvas_pixel curr;
  canvas_pixel font_palette[4] = { 7, 6, 5, 15 };

  memcpy(palette, demon_palette, sizeof(demon_palette));

  if (!this->has_rendered) {
    /* Initialise the canvas randomly */
    for (y = 0; y < dst->h; ++y)
      for (x = 0; x < dst->w; ++x)
        dst->data[canvas_off(dst, x, y)] = rand() & 0xF;

    this->has_rendered = 1;
  } else if (this->time_till_step <= 0) {
    this->time_till_step += 8;
    memcpy(prev, dst->data, sizeof(prev));

    for (y = 0; y < dst->h; ++y) {
      for (x = 0; x < dst->w; ++x) {
        curr = prev[canvas_off(dst, x, y)];
        for (oy = -1; oy <= +1; ++oy) {
          if ((unsigned)oy + y >= dst->h) continue;
          for (ox = -1; ox <= +1; ++ox) {
            if ((unsigned)ox + x >= dst->w) continue;

            if (((curr+1) & 0xF) == prev[canvas_off(dst, x+ox, y+oy)]) {
              dst->data[canvas_off(dst, x, y)] = (curr+1) & 0xF;
              goto next_pixel;
            }
          }
        }

        next_pixel:;
      }
    }
  }

  font_render(dst, &this->font,
              "THE QUICK BROWN FOX JUMPS",
              0, 0, font_palette, 1);
  font_render(dst, &this->font,
              "OVER THE LAZY DOG",
              0, fraktur.em, font_palette, 1);
}

static void test_state_key(test_state* this,
                           SDL_KeyboardEvent* evt) {
  if (SDL_KEYDOWN == evt->type &&
      SDLK_ESCAPE == evt->keysym.sym)
    this->is_alive = 0;
}
