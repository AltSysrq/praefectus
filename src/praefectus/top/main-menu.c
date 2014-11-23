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
#include <time.h>

#include <SDL.h>
#include <libpraefectus/system.h>
#include <libpraefectus/flat-netid.h>
#include <libpraefectus/udp-message-bus/udp-message-bus.h>

#include "bsd.h"

#include "../game-state.h"
#include "../alloc.h"
#include "../global-config.h"
#include "../graphics/canvas.h"
#include "../graphics/console.h"
#include "../ui/menu.h"
#include "../game/context.h"
#include "../game/gameplay.h"
#include "main-menu.h"

#define VERTEX_SERVER "localhost"
#define VERTEX_PORT 44444
static const unsigned short well_known_ports[] = {
  29296, 24946
};

typedef struct main_menu_s main_menu;

static void main_menu_up(void*, menu_level*);
static void main_menu_open_play_menu(void*, menu_level*);
static void main_menu_open_config_menu(void*, menu_level*);
static void main_menu_open_lan_menu(void*, menu_level*);
static void main_menu_open_inet_menu(void*, menu_level*);
static void main_menu_open_key_config_menu(void*, menu_level*);
static void main_menu_open_single_key_config(main_menu*);
static void main_menu_open_error(main_menu*, const char*);
static void main_menu_open_connecting(main_menu*, const char*);
static void main_menu_open_create_new(void*, menu_level*);
static void main_menu_open_discover(void*, menu_level*);
static void main_menu_close_discover(void*, menu_level*);
static void main_menu_do_create_new(void*, menu_level*);
static void main_menu_do_join(void*, menu_level*);
static void main_menu_config_key_up(void*, menu_level*);
static void main_menu_config_key_left(void*, menu_level*);
static void main_menu_config_key_down(void*, menu_level*);
static void main_menu_config_key_right(void*, menu_level*);
static void main_menu_config_key_talk(void*, menu_level*);
static void main_menu_nop(void* this, menu_level* level) { }
static int main_menu_screen_name_accept(int ch) { return !!ch; }
static void main_menu_update_key_config_labels(main_menu*);
static void main_menu_set_active(main_menu*, menu_level*);
static void main_menu_reset_context(main_menu*);
static void main_menu_handle_advert(void*, const praef_umb_advert*,
                                    const PraefNetworkIdentifierPair_t*);

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

static const menu_item main_menu_connecting_items[] = {
  { .type = mit_label, .v = { .label = {
        .label = "Connecting to <set dynamically>..." } } },
};

static const menu_action main_menu_connecting_actions[] = {
  { .label = "Cancel", .action = main_menu_up },
};

static const menu_level main_menu_connecting = {
  .on_accept = main_menu_up, .on_cancel = main_menu_up,
  .title = "Connecting...",
  .items = main_menu_connecting_items,
  .num_items = lenof(main_menu_connecting_items),
  .actions = main_menu_connecting_actions,
  .num_actions = lenof(main_menu_connecting_actions),
  .selected = 1,
};

static const menu_item main_menu_inet_error_items[] = {
  { .type = mit_label, .v = { .label = {
        .label = "Set dynamically" } } },
};

static const menu_action main_menu_inet_error_actions[] = {
  { .label = "Abort", .action = main_menu_up },
  { .label = "Retry", .action = main_menu_open_inet_menu },
  { .label = "Fail", .action = main_menu_up },
};

static const menu_level main_menu_inet_error = {
  .on_accept = main_menu_up, .on_cancel = main_menu_up,
  .title = "Error",
  .items = main_menu_inet_error_items,
  .num_items = lenof(main_menu_inet_error_items),
  .actions = main_menu_inet_error_actions,
  .num_actions = lenof(main_menu_inet_error_actions),
  .selected = 1,
};

static const menu_item main_menu_new_or_join_items[] = {
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Create new game",
        .action = main_menu_open_create_new } } },
  { .type = mit_submenu, .v = { .submenu = {
        .label = "Join existing game",
        .action = main_menu_open_discover } } },
};

static const menu_action main_menu_new_or_join_actions[] = {
  { .label = "Cancel", .action = main_menu_up },
};

static const menu_level main_menu_new_or_join = {
  .on_accept = main_menu_up, .on_cancel = main_menu_up,
  .title = "<Set dynamically>",
  .items = main_menu_new_or_join_items,
  .num_items = lenof(main_menu_new_or_join_items),
  .actions = main_menu_new_or_join_actions,
  .num_actions = lenof(main_menu_new_or_join_actions),
  .selected = 0,
};

static const menu_item main_menu_create_new_items[] = {
  { .type = mit_textfield, .v = { .textfield = {
        .label = "Name of game",
        .text = NULL /* member of main_menu */,
        .text_size = ~0u /* set in main_menu ctor */,
        .accept = main_menu_screen_name_accept } } },
};

static const menu_action main_menu_create_new_actions[] = {
  { .label = "Start", .action = main_menu_do_create_new },
  { .label = "Cancel", .action = main_menu_up },
};

static const menu_level main_menu_create_new = {
  .title = "New Game",
  .on_accept = main_menu_do_create_new,
  .on_cancel = main_menu_up,
  .items = main_menu_create_new_items,
  .num_items = lenof(main_menu_create_new_items),
  .actions = main_menu_create_new_actions,
  .num_actions = lenof(main_menu_create_new_actions),
  .selected = 0,
};

static const menu_item main_menu_discover_items[] = {
  { .type = mit_label, .v = { .label = {
        .label = "Games found:" } } } ,
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 0 } } },
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 1 } } },
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 2 } } },
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 3 } } },
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 4 } } },
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 5 } } },
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 6 } } },
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 7 } } },
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 8 } } },
  { .type = mit_radiolist, .v = { .radio = {
        .label = NULL /* set dynamically */,
        .selected = NULL /* member of main_menu */,
        .ordinal = 9 } } },
};

static const menu_action main_menu_discover_actions[] = {
  { .label = "Join", .action = main_menu_do_join },
  { .label = "Cancel", .action = main_menu_up },
};

static const menu_level main_menu_discover = {
  .title = "Join Game",
  .on_accept = main_menu_do_join,
  .on_cancel = main_menu_close_discover,
  .items = main_menu_discover_items,
  .num_items = lenof(main_menu_discover_items),
  .actions = main_menu_discover_actions,
  .num_actions = lenof(main_menu_discover_actions),
  .selected = 1,
};

typedef enum {
  mmcs_nop = 0,
  mmcs_waiting_for_glboal_address,
  mmcs_idling,
  mmcs_discovering,
  mmcs_joining
} main_menu_connecting_state;

typedef struct {
  unsigned sysid;
  char name[17];
  praef_flat_netid netid;
} main_menu_discovered_game;

struct main_menu_s {
  game_state self;
  game_state* next_state;

  unsigned short canvw, canvh, winw, winh;

  console* cons;
  menu_level top, play, config, key_config, single_key_config,
    connecting, inet_error, new_or_join, create_new, discover;
  menu_level* active;
  menu_item key_config_items[lenof(main_menu_key_config_items)],
    connecting_items[lenof(main_menu_connecting_items)],
    inet_error_items[lenof(main_menu_inet_error_items)],
    discover_items[lenof(main_menu_discover_items)],
    create_new_items[lenof(main_menu_create_new_items)];
  char key_config_labels[lenof(key_configs)][32];
  BoundKey_t* currently_configuring_key;

  char new_game_name[17];
  praef_umb_advert advert;

  main_menu_discovered_game discoveries[10];
  unsigned ms_since_discover_sent;
  unsigned chosen_discovery;

  game_context context;
  praef_message_bus* bus;
  main_menu_connecting_state connecting_state;
  int is_internet_game;
};

game_state* main_menu_new(const canvas* canv,
                          unsigned short winw,
                          unsigned short winh) {
  main_menu* this = zxmalloc(sizeof(main_menu));
  unsigned i;

  this->self.update = (game_state_update_t)main_menu_update;
  this->self.draw = (game_state_draw_t)main_menu_draw;
  this->self.key = (game_state_key_t)main_menu_key;
  this->self.txtin = (game_state_txtin_t)main_menu_txtin;
  this->cons = console_new(canv);
  this->canvw = canv->w;
  this->canvh = canv->h;
  this->winw = winw;
  this->winh = winh;
  this->top = main_menu_top;
  this->play = main_menu_play;
  this->config = main_menu_config;
  this->key_config = main_menu_key_config;
  this->single_key_config = main_menu_single_key_config;
  this->connecting = main_menu_connecting;
  this->inet_error = main_menu_inet_error;
  this->new_or_join = main_menu_new_or_join;
  this->create_new = main_menu_create_new;
  this->discover = main_menu_discover;
  this->active = &this->top;
  memcpy(this->key_config_items, main_menu_key_config_items,
         sizeof(main_menu_key_config_items));
  this->key_config.items = this->key_config_items;
  memcpy(this->connecting_items, main_menu_connecting_items,
         sizeof(main_menu_connecting_items));
  this->connecting.items = this->connecting_items;
  memcpy(this->inet_error_items, main_menu_inet_error_items,
         sizeof(main_menu_inet_error_items));
  this->inet_error.items = this->inet_error_items;
  memcpy(this->discover_items, main_menu_discover_items,
         sizeof(main_menu_discover_items));
  this->discover.items = this->discover_items;
  this->chosen_discovery = -1;
  for (i = 0; i < 10; ++i) {
    this->discover_items[i+1].v.radio.selected = &this->chosen_discovery;
    this->discover_items[i+1].v.radio.label = this->discoveries[i].name;
  }
  memcpy(this->create_new_items, main_menu_create_new_items,
         sizeof(main_menu_create_new_items));
  this->create_new.items = this->create_new_items;
  this->create_new_items[0].v.textfield.text = this->new_game_name;
  this->create_new_items[0].v.textfield.text_size = sizeof(this->new_game_name);

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
  this->new_or_join.cascaded_under = &this->play;
  menu_position_cascade(&this->new_or_join);
  this->create_new.cascaded_under = &this->new_or_join;
  menu_position_cascade(&this->create_new);
  menu_set_minimal_size(&this->create_new, 0, 0);
  this->discover.cascaded_under = &this->new_or_join;
  menu_position_cascade(&this->discover);
  menu_set_minimal_size(&this->discover, 20, 0);

  return (game_state*)this;
}

static void main_menu_delete(main_menu* this) {
  main_menu_reset_context(this);
  free(this->cons);
  free(this);
}

static void main_menu_reset_context(main_menu* this) {
  if (this->context.sys)
    game_context_destroy(&this->context);
  if (this->bus)
    praef_umb_delete(this->bus);

  memset(&this->context, 0, sizeof(this->context));
  this->bus = NULL;
  this->connecting_state = mmcs_nop;
}

static game_state* main_menu_update(main_menu* this, unsigned et) {
  unsigned char packet[512];
  game_state* next_state;

  if (this->bus) {
    switch (this->connecting_state) {
    case mmcs_nop: /* nothing to do */ break;
    case mmcs_joining:
      /* Network cycled by system */
      game_context_update(&this->context, et);
      break;

    default:
      /* Network needs to be pumped manually */
      while ((*this->bus->recv)(packet, sizeof(packet), this->bus));
      break;
    }

    switch (this->connecting_state) {
    case mmcs_waiting_for_glboal_address:
      if (praef_umb_global_address(this->bus)) {
        /* OK, got the global address, vertex connection is ready */
        this->connecting_state = mmcs_idling;
        this->new_or_join.title = "Internet Game";
        this->new_or_join.cascaded_under = &this->play;
        menu_set_minimal_size(&this->new_or_join, 0, 0);
        main_menu_set_active(this, &this->new_or_join);
      }
      break;

    case mmcs_discovering:
      this->ms_since_discover_sent += et;
      if (this->ms_since_discover_sent > 64) {
        praef_umb_send_discovery(this->bus);
        this->ms_since_discover_sent = 0;
      }
      break;

    case mmcs_joining:
      if (praef_ss_ok == this->context.status) {
        main_menu_set_active(this, &this->play);
        this->next_state = gameplay_state_new(
          &this->context, (game_state*)this,
          this->canvw, this->canvh,
          this->winw, this->winh);
        this->connecting_state = mmcs_idling;
      }
      break;

    default: /* Nothing to do */ break;
    }
  }

  if (this->next_state) {
    next_state = this->next_state;
    this->next_state = NULL;
    return next_state;
  } else if (this->active) {
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

static void main_menu_open_error(main_menu* this, const char* str) {
    this->inet_error_items[0].v.label.label = str;
    this->inet_error.cascaded_under = this->active;
    menu_position_cascade(&this->inet_error);
    menu_set_minimal_size(&this->inet_error, 0, 0);
    main_menu_set_active(this, &this->inet_error);
}

static void main_menu_open_connecting(main_menu* this, const char* str) {
  this->connecting_items[0].v.label.label = str;
  this->connecting.cascaded_under = this->active;
  menu_position_cascade(&this->connecting);
  menu_set_minimal_size(&this->connecting, 0, 0);
  main_menu_set_active(this, &this->connecting);
}

static void main_menu_open_lan_menu(void* vthis, menu_level* menu) {
  main_menu_reset_context(THIS);
  THIS->bus = praef_umb_new(PACKAGE_NAME, PACKAGE_VERSION,
                            well_known_ports, lenof(well_known_ports),
                            praef_uiv_ipv4);
  if (!THIS->bus) {
    main_menu_open_error(THIS, "Unable to create network socket.");
    return;
  }

  if (praef_umb_set_spam_firewall(THIS->bus, 1)) {
    main_menu_open_error(THIS, "Unable to broadcast to network.");
    return;
  }

  THIS->connecting_state = mmcs_idling;
  THIS->is_internet_game = 0;
  THIS->new_or_join.title = "LAN Game";
  THIS->new_or_join.cascaded_under = &THIS->play;
  menu_set_minimal_size(&THIS->new_or_join, 0, 0);
  main_menu_set_active(THIS, &THIS->new_or_join);
}

static void main_menu_open_inet_menu(void* vthis, menu_level* menu) {
  main_menu_reset_context(THIS);
  main_menu_set_active(THIS, &THIS->play);
  THIS->bus = praef_umb_new(PACKAGE_NAME, PACKAGE_VERSION,
                            well_known_ports, lenof(well_known_ports),
                            praef_uiv_ipv4);
  if (!THIS->bus) {
    main_menu_open_error(THIS, "Unable to create network socket.");
    return;
  }

  if (praef_umb_set_spam_firewall(THIS->bus, 1)) {
    main_menu_open_error(THIS, "Unable to broadcast to network.");
    return;
  }

  if (praef_umb_lookup_vertex(THIS->bus, VERTEX_SERVER, VERTEX_PORT)) {
    main_menu_open_error(THIS, "Failed to look up server at "VERTEX_SERVER".");
    return;
  }

  praef_umb_set_use_vertex(THIS->bus, 1);
  main_menu_open_connecting(THIS, "Connecting to Internet server...");
  THIS->connecting_state = mmcs_waiting_for_glboal_address;
  THIS->is_internet_game = 1;
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

static void main_menu_open_create_new(void* vthis, menu_level* menu) {
  main_menu_set_active(THIS, &THIS->create_new);
  strlcpy(THIS->new_game_name, (const char*)global_config.screenname.buf,
          sizeof(THIS->new_game_name));
}

static void main_menu_open_discover(void* vthis, menu_level* menu) {
  memset(THIS->discoveries, 0, sizeof(THIS->discoveries));
  THIS->connecting_state = mmcs_discovering;
  THIS->ms_since_discover_sent = 10000; /* force immediate request */
  main_menu_set_active(THIS, &THIS->discover);
  praef_umb_set_listen_advert(THIS->bus, main_menu_handle_advert, THIS);
}

static void main_menu_close_discover(void* vthis, menu_level* menu) {
  praef_umb_set_listen_advert(THIS->bus, NULL, NULL);
  THIS->connecting_state = mmcs_idling;
  main_menu_up(vthis, menu);
}

static void main_menu_handle_advert(void* vthis, const praef_umb_advert* advert,
                                    const PraefNetworkIdentifierPair_t* netid) {
  unsigned i;

  if (advert->data_size >= sizeof(THIS->discoveries[0].name))
    return;

  /* See if already known */
  for (i = 0; i < lenof(THIS->discoveries); ++i)
    if (advert->sysid == THIS->discoveries[i].sysid)
      return;

  /* Find an open slot */
  for (i = 0; i < lenof(THIS->discoveries); ++i)
    if (0 == THIS->discoveries[i].name[0])
      goto found_empty_slot;

  /* All slots taken, oh well. */
  return;

  found_empty_slot:
  THIS->discoveries[i].sysid = advert->sysid;
  memcpy(THIS->discoveries[i].name, (const char*)advert->data,
         advert->data_size);
  THIS->discoveries[i].name[advert->data_size] = 0;
  praef_flat_netid_from_asn1(&THIS->discoveries[i].netid, netid);
}

static void main_menu_do_create_new(void* vthis, menu_level* menu) {
  game_context_init(&THIS->context, THIS->bus,
                    THIS->is_internet_game?
                    praef_umb_global_address(THIS->bus) :
                    praef_umb_local_address(THIS->bus));

  /* This isn't a great way to generate ids, but it should work well enough for
   * this demonstration.
   */
  THIS->advert.sysid = (unsigned)(time(NULL) ^ rand());
  THIS->advert.data_size = strlen(THIS->new_game_name);
  memcpy(THIS->advert.data, THIS->new_game_name,
         strlen(THIS->new_game_name));
  praef_umb_set_advert(THIS->bus, &THIS->advert);
  if (!THIS->is_internet_game)
    praef_umb_set_listen_discover(THIS->bus, 1);

  main_menu_set_active(THIS, &THIS->play);
  praef_system_bootstrap(THIS->context.sys);
  THIS->next_state = gameplay_state_new(&THIS->context, vthis,
                                        THIS->canvw, THIS->canvh,
                                        THIS->winw, THIS->winh);
}

static void main_menu_do_join(void* vthis, menu_level* menu) {
  main_menu_discovered_game* game;

  if (THIS->chosen_discovery >= lenof(THIS->discoveries) ||
      !THIS->discoveries[THIS->chosen_discovery].name[0])
    /* Nothing valid chosen */
    return;

  game = THIS->discoveries + THIS->chosen_discovery;
  praef_umb_set_listen_advert(THIS->bus, NULL, NULL);

  THIS->advert.sysid = game->sysid;
  THIS->advert.data_size = strlen(game->name);
  memcpy(THIS->advert.data, game->name, strlen(game->name));
  praef_umb_set_advert(THIS->bus, &THIS->advert);
  if (!THIS->is_internet_game)
    praef_umb_set_listen_discover(THIS->bus, 1);

  THIS->connecting_state = mmcs_joining;
  game_context_init(&THIS->context, THIS->bus,
                    THIS->is_internet_game?
                    praef_umb_global_address(THIS->bus) :
                    praef_umb_local_address(THIS->bus));
  praef_system_connect(THIS->context.sys,
                       praef_flat_netid_to_asn1(&game->netid));

  main_menu_open_connecting(THIS, "Connecting to peers...");
}

static void main_menu_set_active(main_menu* this, menu_level* level) {
  this->active = level;
  if (level)
    menu_set_console_properties(this->cons, level);
}
