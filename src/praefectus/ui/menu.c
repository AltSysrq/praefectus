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

#include <string.h>
#include <stdio.h>

#include "bsd.h"

#include "../graphics/console.h"
#include "menu.h"

void menu_draw(console* dst, const menu_level* this, int is_active) {
  canvas_pixel title_fg, title_bg, border_fg, window_bg, deselected_fg;
  canvas_pixel selected_fg, selected_bg, accent_fg;
  canvas_pixel textentry_fg, textentry_bg, button_fg, button_bg;
  canvas_pixel button_shadow_fg, button_shadow_bg;
  unsigned x, y, i, actions_width;
  console_cell cell;
  char display[this->w+1];

  if (this->cascaded_under)
    menu_draw(dst, this->cascaded_under, 0);

  if (is_active) {
    title_fg = CONS_VGA_BRIGHT_WHITE;
    title_bg = CONS_VGA_BRIGHT_BLACK;
    border_fg = CONS_VGA_BRIGHT_BLACK;
    window_bg = CONS_VGA_WHITE;
    deselected_fg = CONS_VGA_BLACK;
    selected_fg = CONS_VGA_BRIGHT_WHITE;
    selected_bg = CONS_VGA_BLUE;
    accent_fg = CONS_VGA_BRIGHT_YELLOW;
    textentry_fg = CONS_VGA_BRIGHT_YELLOW;
    textentry_bg = CONS_VGA_BRIGHT_BLACK;
    button_fg = CONS_VGA_BLACK;
    button_bg = CONS_VGA_YELLOW;
    button_shadow_fg = CONS_VGA_BRIGHT_BLACK;
    button_shadow_bg = window_bg;
  } else {
    title_fg = CONS_VGA_WHITE;
    title_bg = CONS_VGA_BLACK;
    border_fg = CONS_VGA_BLACK;
    window_bg = CONS_VGA_BRIGHT_BLACK;
    deselected_fg = CONS_VGA_BLACK;
    selected_fg = CONS_VGA_WHITE;
    selected_bg = CONS_VGA_BLACK;
    accent_fg = CONS_VGA_BRIGHT_BLACK;
    textentry_fg = CONS_VGA_BRIGHT_BLACK;
    textentry_bg = CONS_VGA_BLACK;
    button_fg = CONS_VGA_BLACK;
    button_bg = CONS_VGA_BRIGHT_BLACK;
    button_shadow_fg = CONS_VGA_BLACK;
    button_shadow_bg = window_bg;
  }

  /* Draw borders */
  strlcpy(display, this->title, this->w+1 - 4);
  memset(&cell, 0, sizeof(cell));
  cell.fg = title_fg;
  cell.bg = title_bg;
  console_puts(dst, &cell, this->x+2, this->y, display);
  cell.fg = border_fg;
  cell.bg = window_bg;
  console_putc(dst, &cell, this->x, this->y, CONS_L0210);
  console_putc(dst, &cell, this->x+1, this->y, CONS_L1012);
  console_putc(dst, &cell, this->x+2 + strlen(display), this->y, CONS_L1210);
  console_putc(dst, &cell, this->x+this->w-1, this->y, CONS_L0012);
  console_putc(dst, &cell, this->x+this->w-1, this->y+this->h-1, CONS_L1001);
  console_putc(dst, &cell, this->x, this->y+this->h-1, CONS_L1100);
  for (x = this->x+2+strlen(display)+1; x < this->x+this->w-1; ++x)
    console_putc(dst, &cell, x, this->y, CONS_L0202);
  for (x = this->x+1; x < this->x+this->w-1; ++x)
    console_putc(dst, &cell, x, this->y+this->h-1, CONS_L0101);
  for (y = this->y+1; y < this->y+this->h-1; ++y) {
    console_putc(dst, &cell, this->x, y, CONS_L1010);
    console_putc(dst, &cell, this->x+this->w-1, y, CONS_L1010);
  }

  /* Fill window */
  cell.fg = deselected_fg;
  for (y = this->y+1; y < this->y+this->h-1; ++y)
    for (x = this->x+1; x < this->x+this->w-1; ++x)
      console_putc(dst, &cell, x, y, 0);

  y = this->y + 1;
  for (i = 0; i < this->num_items; ++i, ++y) {
    if (i == this->selected) {
      cell.fg = selected_fg;
      cell.bg = selected_bg;
    } else {
      cell.fg = deselected_fg;
      cell.bg = window_bg;
    }

    for (x = this->x+1; x < this->x+this->w-1; ++x)
      console_putc(dst, &cell, x, y, ' ');

    switch (this->items[i].type) {
    case mit_label:
      strlcpy(display, this->items[i].v.label.label, this->w+1-2);
      console_puts(dst, &cell, this->x+1, y, display);
      break;

    case mit_submenu:
      strlcpy(display, this->items[i].v.submenu.label, this->w+1-3);
      console_puts(dst, &cell, this->x+1, y, display);
      cell.fg = accent_fg;
      console_putc(dst, &cell, this->x+this->w-2, y, 175 /* >> */);
      break;

    case mit_checkbox:
      strlcpy(display, this->items[i].v.checkbox.label, this->w+1-6);
      console_puts(dst, &cell, this->x+5, y, display);
      console_puts(dst, &cell, this->x+1, y,
                   *this->items[i].v.checkbox.is_checked?
                   "[\376]" : "[ ]");
      break;

    case mit_radiobox:
      strlcpy(display, this->items[i].v.radio.label, this->w+1-6);
      console_puts(dst, &cell, this->x+5, y, display);
      console_puts(dst, &cell, this->x+1, y,
                   this->items[i].v.radio.ordinal ==
                   *this->items[i].v.radio.selected?
                   "(\4)" : "( )");
      break;

    case mit_radiolist:
      strlcpy(display, this->items[i].v.radio.label, this->w+1-4);
      console_puts(dst, &cell, this->x+3, y, display);
      if (this->items[i].v.radio.ordinal == *this->items[i].v.radio.selected) {
        cell.fg = accent_fg;
        console_putc(dst, &cell, this->x+1, y, 16 /* |> */);
      }

      break;

    case mit_textfield:
      strlcpy(display, this->items[i].v.textfield.label,
              this->text_field_entry_offset+1);
      console_puts(dst, &cell, this->x+1, y, display);

      cell.fg = textentry_fg;
      cell.bg = textentry_bg;
      memset(display, ' ', this->items[i].v.textfield.text_size);
      memcpy(display, this->items[i].v.textfield.text,
             strlen(this->items[i].v.textfield.text));
      display[this->items[i].v.textfield.text_size] = 0;
      console_puts(dst, &cell, this->x+1+this->text_field_entry_offset, y,
                   display);
      break;
    }
  }

  if (this->actions) {
    actions_width = 0;
    for (i = 0; i < this->num_actions; ++i)
      actions_width += 2 + 2 + strlen(this->actions[i].label);

    ++y;
    x = this->x+1 + this->w/2 - actions_width/2;
    for (i = 0; i < this->num_actions; ++i) {
      if (this->selected == i + this->num_items) {
        cell.fg = selected_fg;
        cell.bg = selected_bg;
      } else {
        cell.fg = button_fg;
        cell.bg = button_bg;
      }

      snprintf(display, sizeof(display), " %s ", this->actions[i].label);
      console_puts(dst, &cell, x, y, display);
      cell.fg = button_shadow_fg;
      cell.bg = button_shadow_bg;
      console_putc(dst, &cell, x+strlen(display), y, CONS_LHBLOCK);
      memset(display, CONS_HHBLOCK, strlen(display));
      console_puts(dst, &cell, x+1, y+1, display);

      x += strlen(display)+2;
    }
  }
}

static void menu_select_previous_item(menu_level* this) {
  unsigned orig = this->selected;

  if (!this->num_items) return;

  do {
    if (this->selected >= this->num_items + !!this->num_actions)
      this->selected = this->num_items - 1;
    else if (!this->selected)
      this->selected = this->num_items - 1 + !!this->num_actions;
    else
      --this->selected;
  } while (orig != this->selected &&
           this->selected < this->num_items &&
           mit_label == this->items[this->selected].type);
}

static void menu_select_next_item(menu_level* this) {
  unsigned orig = this->selected;

  if (!this->num_items) return;

  do {
    if (this->selected+1 >= this->num_items + !!this->num_actions)
      this->selected = 0;
    else
      ++this->selected;
  } while (orig != this->selected &&
           this->selected < this->num_items &&
           mit_label == this->items[this->selected].type);
}

static void menu_select_next_distinct_item_or_action(menu_level* this) {
  unsigned orig = this->selected;
  menu_item_type orig_type = (orig < this->num_items?
                              this->items[orig].type :
                              mit_label);
  int is_on_same_type = 1;

  if (!this->num_items && !this->num_actions) return;

  do {
    if (this->selected+1 >= this->num_items + this->num_actions)
      this->selected = 0;
    else
      ++this->selected;

    if (0 == this->selected || this->selected > this->num_items ||
        orig_type != this->items[this->selected].type)
      is_on_same_type = 0;
  } while (orig != this->selected &&
           (is_on_same_type ||
            (this->selected < this->num_items &&
             mit_label == this->items[this->selected].type)));
}

static void menu_select_previous_action(menu_level* this) {
  if (!this->num_actions) return;

  if (this->selected < this->num_items)
    this->selected = this->num_items;
  else if (this->selected > this->num_items)
    --this->selected;
  else
    this->selected = this->num_items + this->num_actions - 1;
}

static void menu_select_next_action(menu_level* this) {
  if (!this->num_actions) return;

  if (this->selected < this->num_items)
    this->selected = this->num_items + this->num_actions - 1;
  else if (this->selected+1 < this->num_items + this->num_actions)
    ++this->selected;
  else
    this->selected = this->num_items;
}

static void menu_select_first_item(menu_level* this) {
  this->selected = 0;

  while (this->selected < this->num_items &&
         mit_label == this->items[this->selected].type)
    ++this->selected;
}

static void menu_select_first_action_or_last_item(menu_level* this) {
  if (!this->num_actions && !this->num_items) return;

  if (this->num_actions) {
    this->selected = this->num_items;
  } else {
    this->selected = this->num_items - 1;
    while (this->selected > 0 &&
           mit_label == this->items[this->selected].type)
      --this->selected;
  }
}

static int menu_activate(menu_level* this, void* userdata) {
  if (this->selected < this->num_items) {
    switch (this->items[this->selected].type) {
    case mit_checkbox:
      *this->items[this->selected].v.checkbox.is_checked =
        !*this->items[this->selected].v.checkbox.is_checked;
      return 1;

    case mit_radiobox:
    case mit_radiolist:
      *this->items[this->selected].v.radio.selected =
        this->items[this->selected].v.radio.ordinal;
      return mit_radiobox == this->items[this->selected].type;

    case mit_submenu:
      (*this->items[this->selected].v.submenu.action)(
        userdata, this);
      return 1;

    default:
      return 0;
    }
  } else if (this->selected < this->num_items + this->num_actions) {
    (*this->actions[this->selected - this->num_items].action)(
      userdata, this);
    return 1;
  }

  return 0;
}

static void menu_do_primary_action(menu_level* this, void* userdata) {
  (*this->on_accept)(userdata, this);
}

static void menu_cancel(menu_level* this, void* userdata) {
  (*this->on_cancel)(userdata, this);
}

static void menu_delete_backwards_char(menu_level* this, console* cons) {
  if (this->selected > this->num_items ||
      mit_textfield != this->items[this->selected].type)
    return;

  if (!this->items[this->selected].v.textfield.text[0])
    console_bel(cons);

  this->items[this->selected].v.textfield.text[
    strlen(this->items[this->selected].v.textfield.text)-1] = 0;
}

void menu_key(menu_level* this, console* cons, void* userdata,
              SDL_KeyboardEvent* evt) {
  if (SDL_KEYDOWN != evt->type) return;

  switch (evt->keysym.sym) {
  case SDLK_UP:
    menu_select_previous_item(this);
    break;

  case SDLK_DOWN:
    menu_select_next_item(this);
    break;

  case SDLK_TAB:
    menu_select_next_distinct_item_or_action(this);
    break;

  case SDLK_LEFT:
    menu_select_previous_action(this);
    break;

  case SDLK_RIGHT:
    menu_select_next_action(this);
    break;

  case SDLK_HOME:
  case SDLK_PAGEUP:
    menu_select_first_item(this);
    break;

  case SDLK_END:
  case SDLK_PAGEDOWN:
    menu_select_first_action_or_last_item(this);
    break;

  case SDLK_SPACE:
    menu_activate(this, userdata);
    break;

  case SDLK_RETURN:
    if (!menu_activate(this, userdata))
      menu_do_primary_action(this, userdata);
    break;

  case SDLK_ESCAPE:
    menu_cancel(this, userdata);
    break;

  case SDLK_BACKSPACE:
    menu_delete_backwards_char(this, cons);
    break;
  }

  menu_set_console_properties(cons, this);
}

void menu_set_console_properties(console* cons, const menu_level* this) {
  if (this->selected < this->num_items &&
      mit_textfield == this->items[this->selected].type) {
    cons->show_cursor = 1;
    cons->cursor_x = this->x + 1 + this->text_field_entry_offset +
      strlen(this->items[this->selected].v.textfield.text);
    cons->cursor_y = this->y + 1 + this->selected;
    /* This usage doesn't play very well with non-trivial input methods. But
     * any non-trivial input method is either something very exotic (eg,
     * stenography), or produces characters not present in CP 437 anyway.
     */
    SDL_StartTextInput();
  } else {
    cons->show_cursor = 0;
    SDL_StopTextInput();
  }
}

static const char menu_utf8_cp437_table[] =
#include "utf8-cp437.inc"
  ;

static unsigned char menu_utf8_to_cp437(const char* utf8) {
  unsigned i;
  const char* cursor = menu_utf8_cp437_table;

  for (i = 1; i < 256; ++i)
    if (!strcmp(cursor, utf8))
      return i;
    else
      cursor += strlen(cursor)+1;

  return 0;
}

void menu_txtin(menu_level* this, console* cons, SDL_TextInputEvent* evt) {
  if (this->selected < this->num_items &&
      mit_textfield == this->items[this->selected].type) {
    unsigned char ch[2];
    ch[0] = menu_utf8_to_cp437(evt->text);
    ch[1] = 0;
    if (ch[0] && (*this->items[this->selected].v.textfield.accept)(ch[0]) &&
        strlen(this->items[this->selected].v.textfield.text)+1 <
        this->items[this->selected].v.textfield.text_size)
      strlcat(this->items[this->selected].v.textfield.text, (char*)ch,
              this->items[this->selected].v.textfield.text_size);
    else
      console_bel(cons);
  }

  menu_set_console_properties(cons, this);
}

void menu_set_minimal_size(menu_level* this, unsigned min_w, unsigned min_h) {
  unsigned i, actions_width;

  this->w = min_w;
  this->h = min_h;
  this->text_field_entry_offset = 0;

#define ACCMAX(acc, val) do { if ((val) > (acc)) (acc) = (val); } while (0)

  for (i = 0; i < this->num_items; ++i)
    if (mit_textfield == this->items[i].type)
      ACCMAX(this->text_field_entry_offset,
             strlen(this->items[i].v.textfield.label) + 1);

  for (i = 0; i < this->num_items; ++i) {
    switch (this->items[i].type) {
    case mit_label:
      ACCMAX(this->w, strlen(this->items[i].v.label.label)+2);
      break;

    case mit_submenu:
      ACCMAX(this->w, strlen(this->items[i].v.submenu.label)+3);
      break;

    case mit_checkbox:
      ACCMAX(this->w, strlen(this->items[i].v.checkbox.label)+6);
      break;

    case mit_radiobox:
      ACCMAX(this->w, strlen(this->items[i].v.radio.label)+6);
      break;

    case mit_radiolist:
      ACCMAX(this->w, strlen(this->items[i].v.radio.label)+4);
      break;

    case mit_textfield:
      ACCMAX(this->w, 2 + this->text_field_entry_offset +
             this->items[i].v.textfield.text_size);
      break;
    }
  }

    actions_width = 2;
    for (i = 0; i < this->num_actions; ++i)
      actions_width += 2 + 3 + strlen(this->actions[i].label);
    ACCMAX(this->w, actions_width);
    ACCMAX(this->w, strlen(this->title)+4);

    ACCMAX(this->h, this->num_items + 2 + (this->actions? 3 : 0));
#undef ACCMAX
}

void menu_position_cascade(menu_level* this) {
  this->x = this->cascaded_under->x + 1;
  this->y = this->cascaded_under->y + 1;
}
