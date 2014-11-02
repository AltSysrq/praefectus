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
#include "../graphics/console.h"
#include "../graphics/crt.h"
#include "../graphics/font.h"
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
} test_state;

static game_state* test_state_update(test_state*, unsigned);
static void test_state_draw(test_state*, console*, crt_colour*);
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

/* Hack to test BEL */
static console* scons;
static void test_state_draw(test_state* this, console* dst,
                            crt_colour* palette) {
  console_cell cell;
  unsigned x, y;

  scons = dst;

  memset(&cell, 0, sizeof(cell));
  cell.fg = CONS_VGA_BRIGHT_WHITE;
  console_puts(dst, &cell, 2, 0, "Test");
  cell.fg = CONS_VGA_BRIGHT_BLACK;
  console_putc(dst, &cell, 0, 0, CONS_L0110);
  console_putc(dst, &cell, 1, 0, CONS_L2021);
  console_putc(dst, &cell, 6, 0, CONS_L2120);
  for (x = 7; x < 15; ++x)
    console_putc(dst, &cell, x, 0, CONS_L0101);
  console_putc(dst, &cell, 15, 0, CONS_L0011);
  for (y = 1; y < 6; ++y) {
    console_putc(dst, &cell, 0, y, CONS_L1010);
    console_putc(dst, &cell, 15, y, CONS_L1010);
  }
  console_putc(dst, &cell, 0, 6, CONS_L1100);
  console_putc(dst, &cell, 15, 6, CONS_L1001);
  for (x = 1; x < 15; ++x)
    console_putc(dst, &cell, x, 6, CONS_L0101);

  cell.fg = CONS_VGA_WHITE;
  console_puts(dst, &cell, 1, 1, "Hello world");
  cell.bg = CONS_VGA_YELLOW;
  console_puts(dst, &cell, 1, 2, "With background");
  cell.blink = 1;
  console_puts(dst, &cell, 1, 3, "Blink");
  cell.reverse_video = 1;
  console_puts(dst, &cell, 1, 4, "Reverse video");

  dst->show_cursor = 1;
  dst->cursor_x = 2;
  dst->cursor_y = 2;

  crt_default_palette(palette);
}

static void test_state_key(test_state* this,
                           SDL_KeyboardEvent* evt) {
  if (SDL_KEYDOWN == evt->type &&
      SDLK_ESCAPE == evt->keysym.sym)
    this->is_alive = 0;
  else
    console_bel(scons);
}
