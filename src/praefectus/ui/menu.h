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
#ifndef UI_MENU_H_
#define UI_MENU_H_

#include <SDL.h>

#include "../graphics/console.h"

__BEGIN_DECLS

typedef struct menu_level_s menu_level;

typedef enum {
  mit_label,
  mit_submenu,
  mit_checkbox,
  mit_radiobox,
  mit_radiolist,
  mit_textfield
} menu_item_type;

typedef struct {
  menu_item_type type;

  union {
    struct {
      const char* label;
    } label;

    struct {
      const char* label;
      void (*action)(void*, menu_level*);
    } submenu;

    struct {
      const char* label;
      int* is_checked;
    } checkbox;

    struct {
      const char* label;
      unsigned* selected;
      unsigned ordinal;
    } radio;

    struct {
      const char* label;
      char* text;
      unsigned text_size;
      int (*accept)(int);
    } textfield;
  } v;
} menu_item;

typedef struct {
  const char* label;
  void (*action)(void*, menu_level*);
} menu_action;

struct menu_level_s {
  unsigned x, y, w, h;
  unsigned text_field_entry_offset;
  unsigned selected;

  void (*on_accept)(void*, menu_level*);
  void (*on_cancel)(void*, menu_level*);

  const char* title;

  const menu_item* items;
  unsigned num_items;

  const menu_action* actions;
  unsigned num_actions;

  const struct menu_level_s* cascaded_under;
};

void menu_draw(console*, const menu_level*, int is_active);
void menu_key(menu_level*, console*, void* userdata, SDL_KeyboardEvent*);
void menu_txtin(menu_level*, console*, SDL_TextInputEvent*);
void menu_set_console_properties(console*, const menu_level*);

void menu_set_minimal_size(menu_level*, unsigned min_w, unsigned min_h);
void menu_position_cascade(menu_level*);

__END_DECLS

#endif /* UI_MENU_H_ */
