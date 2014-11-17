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

#include "bsd.h"

#include "../game-state.h"
#include "../alloc.h"
#include "../global-config.h"
#include "../graphics/canvas.h"
#include "../graphics/console.h"
#include "../ui/menu.h"
#include "main-menu.h"

typedef struct main_menu_s main_menu;

static void main_menu_up(void*, menu_level*);
static void main_menu_open_play_menu(void*, menu_level*);
static void main_menu_open_config_menu(void*, menu_level*);
static void main_menu_open_lan_menu(void*, menu_level*);
static void main_menu_open_inet_menu(void*, menu_level*);
static void main_menu_open_key_config_menu(void*, menu_level*);
static void main_menu_open_single_key_config(main_menu*);
static void main_menu_config_key_up(void*, menu_level*);
static void main_menu_config_key_left(void*, menu_level*);
static void main_menu_config_key_down(void*, menu_level*);
static void main_menu_config_key_right(void*, menu_level*);
static void main_menu_config_key_talk(void*, menu_level*);
static void main_menu_nop(void* this, menu_level* level) { }
static int main_menu_screen_name_accept(int ch) { return !!ch; }
static void main_menu_update_key_config_labels(main_menu*);
static void main_menu_set_active(main_menu*, menu_level*);

static game_state* main_menu_update(main_menu*, unsigned);
static void main_menu_draw(main_menu*, canvas*, crt_colour*);
static void main_menu_key(main_menu*, SDL_KeyboardEvent*);
static void main_menu_txtin(main_menu*, SDL_TextInputEvent*);

#define lenof(x) (sizeof(x)/sizeof(x[0]))

static const menu_item main_menu_top_items[] = {
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Play",
        .action = main_menu_open_play_menu } } },
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Configuration",
        .action = main_menu_open_config_menu } } },
};

static const menu_action main_menu_top_actions[] = {
  { .label = "Quit",
    .action = main_menu_up },
};

static const menu_level main_menu_top = {
  .on_accept = main_menu_nop,
  .on_cancel = main_menu_up,
  .title = "Main Menu",
  .items = main_menu_top_items,
  .num_items = lenof(main_menu_top_items),
  .actions = main_menu_top_actions,
  .num_actions = lenof(main_menu_top_actions),
};

static const menu_item main_menu_play_items[] = {
  { .type = mit_textfield, .v = { .textfield = {
        .label = "Screen Name",
        .accept = main_menu_screen_name_accept,
        .text = global_config_screen_name,
        .text_size = sizeof(global_config_screen_name) } } },
  { .type = mit_label, .v = { .label = { .label = "" } } },
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Local Area Network",
        .action = main_menu_open_lan_menu } } },
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Internet",
        .action = main_menu_open_inet_menu } } },
};

static const menu_action main_menu_play_actions[] = {
  { .label = "Cancel", .action = main_menu_up },
};

static const menu_level main_menu_play = {
  .on_accept = main_menu_nop,
  .on_cancel = main_menu_up,
  .title = "Play",
  .items = main_menu_play_items,
  .num_items = lenof(main_menu_play_items),
  .actions = main_menu_play_actions,
  .num_actions = lenof(main_menu_play_actions),
};

static const menu_item main_menu_config_items[] = {
  { .type = mit_textfield, .v = { .textfield = {
        .label = "Screen Name",
        .accept = main_menu_screen_name_accept,
        .text = global_config_screen_name,
        .text_size = sizeof(global_config_screen_name) } } },
  { .type = mit_label, .v = { .label = { .label = "" } } },
  { .type = mit_label, .v = { .label = { .label = "Flash Effects" } } },
  { .type = mit_radiobox, .v = { .radio = {
        .label = "None",
        .selected = (unsigned*)&global_config.bel,
        .ordinal = PraefectusConfiguration__bel_none } } },
  { .type = mit_radiobox, .v = { .radio = {
        .label = "Borders only",
        .selected = (unsigned*)&global_config.bel,
        .ordinal = PraefectusConfiguration__bel_smallflash } } },
  { .type = mit_radiobox, .v = { .radio = {
        .label = "Whole Screen",
        .selected = (unsigned*)&global_config.bel,
        .ordinal = PraefectusConfiguration__bel_flash } } },
  { .type = mit_radiobox, .v = { .radio = {
        .label = "Strobe",
        .selected = (unsigned*)&global_config.bel,
        .ordinal = PraefectusConfiguration__bel_strobe } } },
  { .type = mit_label, .v = { .label = { .label = "" } } },
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Key Configuration",
        .action = main_menu_open_key_config_menu } } },
};

static const menu_action main_menu_config_actions[] = {
  { .label = "Close", .action = main_menu_up },
};

static const menu_level main_menu_config = {
  .on_accept = main_menu_up,
  .on_cancel = main_menu_up,
  .title = "Configuration",
  .items = main_menu_config_items,
  .num_items = lenof(main_menu_config_items),
  .actions = main_menu_config_actions,
  .num_actions = lenof(main_menu_config_actions),
};

static const menu_item main_menu_key_config_items[] = {
  /* Labels here are generated dynamically; the constant strings are only for
   * illustration.
   */
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Move Up",
        .action = main_menu_config_key_up } } },
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Move Left",
        .action = main_menu_config_key_left } } },
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Move Down",
        .action = main_menu_config_key_down } } },
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Move Right",
        .action = main_menu_config_key_right } } },
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Talk",
        .action = main_menu_config_key_talk } } },
};

static const struct {
  const char* name;
  size_t offset;
} key_configs[lenof(main_menu_key_config_items)] = {
  { "Move Up", offsetof(ControlsConfiguration_t, up) },
  { "Move Left", offsetof(ControlsConfiguration_t, left) },
  { "Move Down", offsetof(ControlsConfiguration_t, down) },
  { "Move Right", offsetof(ControlsConfiguration_t, right) },
  { "Talk", offsetof(ControlsConfiguration_t, talk) },
};

static const menu_action main_menu_key_config_actions[] = {
  { .label = "Close", .action = main_menu_up },
};

static const menu_level main_menu_key_config = {
  .on_accept = main_menu_up,
  .on_cancel = main_menu_up,
  .title = "Key Configuration",
  .items = main_menu_key_config_items,
  .num_items = lenof(main_menu_key_config_items),
  .actions = main_menu_key_config_actions,
  .num_actions = lenof(main_menu_key_config_actions),
};

static const menu_item main_menu_single_key_config_items[] = {
  { .type = mit_label, .v = { .label = {
        .label = "Press key to bind, or escape to cancel." } } },
};

static const menu_level main_menu_single_key_config = {
  .on_accept = NULL, .on_cancel = NULL, /* should never be called */
  .title = "Configure Key",
  .items = main_menu_single_key_config_items,
  .num_items = lenof(main_menu_key_config_actions),
  .actions = NULL,
  .num_actions = 0,
  .selected = 1,
};

struct main_menu_s {
  game_state self;

  console* cons;
  menu_level top, play, config, key_config, single_key_config;
  menu_level* active;
  menu_item key_config_items[lenof(main_menu_key_config_items)];
  char key_config_labels[lenof(key_configs)][32];
  BoundKey_t* currently_configuring_key;
};

game_state* main_menu_new(const canvas* canv) {
  main_menu* this = zxmalloc(sizeof(main_menu));

  this->self.update = (game_state_update_t)main_menu_update;
  this->self.draw = (game_state_draw_t)main_menu_draw;
  this->self.key = (game_state_key_t)main_menu_key;
  this->self.txtin = (game_state_txtin_t)main_menu_txtin;
  this->cons = console_new(canv);
  this->top = main_menu_top;
  this->play = main_menu_play;
  this->config = main_menu_config;
  this->key_config = main_menu_key_config;
  this->single_key_config = main_menu_single_key_config;
  this->active = &this->top;
  memcpy(this->key_config_items, main_menu_key_config_items,
         sizeof(main_menu_key_config_items));
  this->key_config.items = this->key_config_items;

  this->top.x = 2;
  this->top.y = 2;
  menu_set_minimal_size(&this->top, 0, 0);
  menu_set_minimal_size(&this->play, 0, 0);
  this->play.cascaded_under = &this->top;
  menu_position_cascade(&this->play);
  menu_set_minimal_size(&this->config, 0, 0);
  this->config.cascaded_under = &this->top;
  menu_position_cascade(&this->config);
  menu_set_minimal_size(&this->key_config, 0, 0);
  this->key_config.cascaded_under = &this->config;
  menu_position_cascade(&this->key_config);
  menu_set_minimal_size(&this->single_key_config, 0, 0);
  this->single_key_config.cascaded_under = &this->key_config;
  menu_position_cascade(&this->single_key_config);

  return (game_state*)this;
}

static void main_menu_delete(main_menu* this) {
  free(this->cons);
  free(this);
}

static game_state* main_menu_update(main_menu* this, unsigned et) {
  if (this->active) {
    return (game_state*)this;
  } else {
    main_menu_delete(this);
    return NULL;
  }
}

static void main_menu_draw(main_menu* this, canvas* dst, crt_colour* palette) {
  switch (global_config.bel) {
  case PraefectusConfiguration__bel_none:
    this->cons->bel_behaviour = cbb_noop;
    break;
  case PraefectusConfiguration__bel_smallflash:
    this->cons->bel_behaviour = cbb_flash_extremities;
    break;

  case PraefectusConfiguration__bel_flash:
    this->cons->bel_behaviour = cbb_flash;
    break;

  case PraefectusConfiguration__bel_strobe:
    this->cons->bel_behaviour = cbb_strobe;
    break;
  }

  crt_default_palette(palette);
  console_clear(this->cons);
  if (this->active) menu_draw(this->cons, this->active, 1);
  console_render(dst, this->cons);
}

static void main_menu_key(main_menu* this, SDL_KeyboardEvent* evt) {
  if (this->currently_configuring_key) {
    if (SDL_KEYDOWN == evt->type) {
      if (SDLK_ESCAPE != evt->keysym.sym)
        *this->currently_configuring_key = evt->keysym.sym;
      this->currently_configuring_key = NULL;
      main_menu_open_key_config_menu(this, NULL);
    }
  } else {
    if (this->active)
      menu_key(this->active, this->cons, this, evt);
  }
}

static void main_menu_txtin(main_menu* this, SDL_TextInputEvent* evt) {
  if (this->active)
    menu_txtin(this->active, this->cons, evt);
}

#define THIS ((main_menu*)vthis)

static void main_menu_up(void* vthis, menu_level* menu) {
  main_menu_set_active(THIS, (menu_level*)menu->cascaded_under);
}

static void main_menu_open_play_menu(void* vthis, menu_level* menu) {
  main_menu_set_active(THIS, &THIS->play);
}

static void main_menu_open_config_menu(void* vthis, menu_level* menu) {
  main_menu_set_active(THIS, &THIS->config);
}

static void main_menu_open_lan_menu(void* vthis, menu_level* menu) {
  /* TODO */
}

static void main_menu_open_inet_menu(void* vthis, menu_level* menu) {
  /* TODO */
}

static void main_menu_open_key_config_menu(void* vthis, menu_level* menu) {
  main_menu_update_key_config_labels(THIS);
  main_menu_set_active(THIS, &THIS->key_config);
}

static void main_menu_open_single_key_config(main_menu* this) {
  main_menu_set_active(this, &this->single_key_config);
}

static void main_menu_config_key_up(void* vthis, menu_level* menu) {
  THIS->currently_configuring_key = &global_config.controls.up;
  main_menu_open_single_key_config(THIS);
}

static void main_menu_config_key_left(void* vthis, menu_level* menu) {
  THIS->currently_configuring_key = &global_config.controls.left;
  main_menu_open_single_key_config(THIS);
}

static void main_menu_config_key_down(void* vthis, menu_level* menu) {
  THIS->currently_configuring_key = &global_config.controls.down;
  main_menu_open_single_key_config(THIS);
}

static void main_menu_config_key_right(void* vthis, menu_level* menu) {
  THIS->currently_configuring_key = &global_config.controls.right;
  main_menu_open_single_key_config(THIS);
}

static void main_menu_config_key_talk(void* vthis, menu_level* menu) {
  THIS->currently_configuring_key = &global_config.controls.talk;
  main_menu_open_single_key_config(THIS);
}

static void main_menu_update_key_config_labels(main_menu* this) {
  unsigned i;
  const char* key_name;
  char numeric[8];
  BoundKey_t bound;

  for (i = 0; i < lenof(key_configs); ++i) {
    bound = *(BoundKey_t*)(((char*)&global_config.controls) +
                           key_configs[i].offset);
    key_name = SDL_GetKeyName(bound);
    if (!key_name || !*key_name) {
      snprintf(numeric, sizeof(numeric), "<%04X>", (unsigned)bound);
      key_name = numeric;
    }

    snprintf(this->key_config_labels[i], sizeof(this->key_config_labels[i]),
             "%12s: %s", key_configs[i].name, key_name);
    this->key_config_items[i].v.submenu.label = this->key_config_labels[i];
  }

  menu_set_minimal_size(&this->key_config, this->key_config.w,
                        this->key_config.h);
}

static void main_menu_set_active(main_menu* this, menu_level* level) {
  this->active = level;
  if (level)
    menu_set_console_properties(this->cons, level);
}
