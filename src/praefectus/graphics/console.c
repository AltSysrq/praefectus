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

#include <stddef.h>

#include "font.h"
#include "console.h"
#include "../alloc.h"

#define STROBE_INTERVAL 3
#define BEL_DURATION 9
#define CURSOR_BLINK_INTERVAL 6
#define CHAR_BLINK_INTERVAL 30

console* console_new(const canvas* canv) {
  unsigned w, h;
  console* this;

  w = canv->w / FONT_CHARW;
  h = canv->h / FONT_CHARH;
  this = zxmalloc(offsetof(console, c) +
                  sizeof(console_cell) * w * h);
  this->w = w;
  this->h = h;
  /* Start frameno advanced so last_bel is not within the bel duration. */
  this->frameno = 1000;
  this->bel_behaviour = cbb_flash;
  return this;
}

void console_delete(console* this) {
  free(this);
}

console_cell* console_ca(console* this, unsigned x, unsigned y) {
  return this->c + this->w*y + x;
}

void console_bel(console* this) {
  if (this->frameno - this->last_bel >= STROBE_INTERVAL*2)
    this->last_bel = this->frameno;
}

void console_clear(console* this) {
  unsigned i;
  memset(this->c, 0, sizeof(console_cell) * this->w * this->h);
  for (i = 0; i < (unsigned)this->w * (unsigned)this->h; ++i)
    this->c[i].fg = CONS_VGA_WHITE;
}

void console_puts(console* this, const console_cell* template,
                  unsigned x, unsigned y, const char* str) {
  unsigned off = this->w*y + x;
  while (*str && x < this->w) {
    this->c[off] = *template;
    this->c[off].ch = *str;
    ++off, ++str, ++x;
  }
}

void console_render(canvas* dst, console* this) {
  int global_reverse_video = 0;
  int extremity_reverse_video = 0;
  int row_reverse_video;
  int show_cursor = (this->show_cursor &&
                     ((this->frameno / CURSOR_BLINK_INTERVAL) & 1));
  unsigned x, y, ch, ox, oy;
  canvas_pixel fg, bg;

  switch (this->bel_behaviour) {
  case cbb_noop: break;

  case cbb_flash_extremities:
    extremity_reverse_video = (this->frameno - this->last_bel < BEL_DURATION);
    break;

  case cbb_flash:
    global_reverse_video = (this->frameno - this->last_bel < BEL_DURATION);
    break;

  case cbb_strobe:
    global_reverse_video = (this->frameno - this->last_bel < BEL_DURATION &&
                            this->frameno - this->last_bel > STROBE_INTERVAL);
    break;
  }

  ch = 0;
  for (y = 0; y < this->h; ++y) {
    row_reverse_video = global_reverse_video ||
      ((y == 0 || y == (unsigned)this->h-1) && extremity_reverse_video);

    for (x = 0; x < this->w; ++x, ++ch) {
      if (row_reverse_video ^ this->c[ch].reverse_video ^
          (x == this->mouse_x && y == this->mouse_y)) {
        fg = this->c[ch].bg;
        bg = this->c[ch].fg;
      } else {
        fg = this->c[ch].fg;
        bg = this->c[ch].bg;
      }

      for (oy = 0; oy < FONT_CHARH; ++oy)
        for (ox = 0; ox < FONT_CHARW; ++ox)
          dst->data[canvas_off(dst, x*FONT_CHARW+ox, y*FONT_CHARH+oy)] = bg;

      if (!this->c[ch].blink || ((this->frameno / CHAR_BLINK_INTERVAL) & 1))
        font_renderch(dst, x*FONT_CHARW, y*FONT_CHARH, this->c[ch].ch, fg);

      if (show_cursor && x == this->cursor_x && y == this->cursor_y)
        font_renderch(dst, x*FONT_CHARW, y*FONT_CHARH, '_', fg);
    }
  }

  ++this->frameno;
}
