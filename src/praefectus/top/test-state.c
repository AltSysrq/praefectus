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
#include "../game-state.h"
#include "test-state.h"

typedef struct {
  game_state self;
  int is_alive;
} test_state;

static game_state* test_state_update(test_state*, unsigned);
static void test_state_draw(test_state*, canvas*);
static void test_state_key(test_state*, SDL_KeyboardEvent*);

game_state* test_state_new(void) {
  test_state* this = xmalloc(sizeof(test_state));

  memset(this, 0, sizeof(test_state));
  this->self.update = (game_state_update_t)test_state_update;
  this->self.draw = (game_state_draw_t)test_state_draw;
  this->self.key = (game_state_key_t)test_state_key;
  this->is_alive = 1;
  return (game_state*)this;
}

void test_state_delete(game_state* this) {
  free(this);
}

static game_state* test_state_update(test_state* this, unsigned et) {
  if (this->is_alive) {
    return (game_state*)this;
  } else {
    test_state_delete((game_state*)this);
    return NULL;
  }
}

static void test_state_draw(test_state* this, canvas* dst) {
}

static void test_state_key(test_state* this,
                           SDL_KeyboardEvent* evt) {
  if (SDL_KEYDOWN == evt->type &&
      SDLK_ESCAPE == evt->keysym.sym)
    this->is_alive = 0;
}
