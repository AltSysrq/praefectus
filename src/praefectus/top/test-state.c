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
#include "../ui/menu.h"
#include "../game-state.h"
#include "test-state.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

static int anything(int i) { return 1; }

typedef struct {
  game_state self;
  int is_alive;
  console* cons;
  menu_level* active_menu;
  menu_level top_menu;
} test_state;

static int checkbox_state;
static unsigned radio_state;
static char textbuf[8];
const menu_item items_on_top_menu[] = {
  { .type = mit_label,
    .v = {
      .label = {
        .label = "This is a label."
      }
    }
  },
  { .type = mit_submenu,
    .v = {
      .submenu = {
        .label = "Configure",
        .action = /* TODO */ NULL
      }
    }
  },
  { .type = mit_checkbox,
    .v = {
      .checkbox = {
        .label = "Checkbox",
        .is_checked = &checkbox_state
      }
    }
  },
  { .type = mit_radiobox,
    .v = {
      .radio = {
        .label = "Radiobox",
        .ordinal = 0,
        .selected = &radio_state
      }
    }
  },
  { .type = mit_radiolist,
    .v = {
      .radio = {
        .label = "Radiolist",
        .ordinal = 0,
        .selected = &radio_state
      }
    }
  },
  { .type = mit_textfield,
    .v = {
      .textfield = {
        .label = "Text",
        .accept = anything,
        .text = textbuf,
        .text_size = sizeof(textbuf)
      }
    }
  },
};

const menu_action actions_on_top_menu[] = {
  { .label = "Abort" },
  { .label = "Retry" },
  { .label = "Fail" },
};

static void do_nothing(void* u, menu_level* m) { }
static void quit(void*, menu_level*);
const menu_level top_menu = {
  .selected = 1,
  .x = 3, .y = 3,
  .title = "Main Menu",
  .items = items_on_top_menu,
  .num_items = sizeof(items_on_top_menu) / sizeof(menu_item),
  .actions = actions_on_top_menu,
  .num_actions = sizeof(actions_on_top_menu) / sizeof(menu_action),

  .on_accept = do_nothing,
  .on_cancel = quit
};

static game_state* test_state_update(test_state*, unsigned);
static void test_state_draw(test_state*, console*, crt_colour*);
static void test_state_key(test_state*, SDL_KeyboardEvent*);
static void test_state_txtin(test_state*, SDL_TextInputEvent*);

game_state* test_state_new(console* cons) {
  test_state* this = xmalloc(sizeof(test_state));

  memset(this, 0, sizeof(test_state));
  this->self.update = (game_state_update_t)test_state_update;
  this->self.draw = (game_state_draw_t)test_state_draw;
  this->self.key = (game_state_key_t)test_state_key;
  this->self.txtin = (game_state_txtin_t)test_state_txtin;
  this->is_alive = 1;
  this->top_menu = top_menu;
  this->active_menu = &this->top_menu;
  this->cons = cons;

  menu_set_minimal_size(&this->top_menu, 0, 0);

  return (game_state*)this;
}

void test_state_delete(game_state* this) {
  free(this);
}

static void quit(void* userdata, menu_level* menu) {
  ((test_state*)userdata)->is_alive = 0;
}

static game_state* test_state_update(test_state* this, unsigned et) {
  if (this->is_alive) {
    return (game_state*)this;
  } else {
    test_state_delete((game_state*)this);
    return NULL;
  }
}

static void test_state_draw(test_state* this, console* dst,
                            crt_colour* palette) {
  crt_default_palette(palette);
  menu_draw(dst, this->active_menu, 1);
}

static void test_state_key(test_state* this,
                           SDL_KeyboardEvent* evt) {
  menu_key(this->active_menu, this->cons, this, evt);
}

static void test_state_txtin(test_state* this,
                             SDL_TextInputEvent* evt) {
  menu_txtin(this->active_menu, this->cons, evt);
}
