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
#ifndef GRAPHICS_CONSOLE_H_
#define GRAPHICS_CONSOLE_H_

#include "canvas.h"

typedef enum {
  cbb_strobe,
  cbb_flash,
  cbb_flash_extremities,
  cbb_noop
} console_bel_behaviour;

typedef struct {
  unsigned char ch;
  canvas_pixel fg, bg;
  unsigned char reverse_video   : 1;
  unsigned char blink           : 1;
} console_cell;

typedef struct {
  unsigned short w, h;
  unsigned short mouse_x, mouse_y;
  unsigned short cursor_x, cursor_y;
  int show_mouse, show_cursor;

  unsigned frameno;
  unsigned last_bel;
  console_bel_behaviour bel_behaviour;

  console_cell c[FLEXIBLE_ARRAY_MEMBER];
} console;

#define CONS_VGA_BLACK          ((canvas_pixel)CP_GREY   + 0)
#define CONS_VGA_RED            ((canvas_pixel)CP_RED    + CP_SIZE*6/10)
#define CONS_VGA_YELLOW         ((canvas_pixel)CP_ORANGE + CP_SIZE*6/10)
#define CONS_VGA_GREEN          ((canvas_pixel)CP_GREEN  + CP_SIZE*6/10)
#define CONS_VGA_CYAN           ((canvas_pixel)CP_CYAN   + CP_SIZE*6/10)
#define CONS_VGA_BLUE           ((canvas_pixel)CP_BLUE   + CP_SIZE*6/10)
#define CONS_VGA_MAGENTA        ((canvas_pixel)CP_MAGENTA+ CP_SIZE*6/10)
#define CONS_VGA_WHITE          ((canvas_pixel)CP_GREY   + CP_SIZE*6/10)
#define CONS_VGA_BRIGHT_BLACK   ((canvas_pixel)CP_GREY   + CP_SIZE*4/10)
#define CONS_VGA_BRIGHT_RED     ((canvas_pixel)CP_RED    + CP_SIZE-1)
#define CONS_VGA_BRIGHT_YELLOW  ((canvas_pixel)CP_YELLOW + CP_SIZE-1)
#define CONS_VGA_BRIGHT_GREEN   ((canvas_pixel)CP_GREEN  + CP_SIZE-1)
#define CONS_VGA_BRIGHT_CYAN    ((canvas_pixel)CP_CYAN   + CP_SIZE-1)
#define CONS_VGA_BRIGHT_BLUE    ((canvas_pixel)CP_BLUE   + CP_SIZE-1)
#define CONS_VGA_BRIGHT_WHITE   ((canvas_pixel)CP_WHITE  + CP_SIZE-1)

console* console_new(const canvas*);
void console_delete(console*);
console_cell* console_ca(console*, unsigned x, unsigned y);
void console_puts(console*, const console_cell* template,
                  unsigned x0, unsigned y, const char*);
void console_bel(console*);
void console_clear(console*);
void console_render(canvas*, console*);

#endif /* GRAPHICS_CONSOLE_H_ */
