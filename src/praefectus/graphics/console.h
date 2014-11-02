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
#define CONS_VGA_BRIGHT_WHITE   ((canvas_pixel)CP_GREY   + CP_SIZE-1)

#define CONS_STIP1_BLOCK ((unsigned char)176)
#define CONS_STIP2_BLOCK ((unsigned char)177)
#define CONS_STIP3_BLOCK ((unsigned char)178)
#define CONS_L1010 ((unsigned char)179)
#define CONS_L1011 ((unsigned char)180)
#define CONS_L1012 ((unsigned char)181)
#define CONS_L2021 ((unsigned char)182)
#define CONS_L0021 ((unsigned char)183)
#define CONS_L0012 ((unsigned char)184)
#define CONS_L2022 ((unsigned char)185)
#define CONS_L2020 ((unsigned char)186)
#define CONS_L0022 ((unsigned char)187)
#define CONS_L2002 ((unsigned char)188)
#define CONS_L2001 ((unsigned char)189)
#define CONS_L1002 ((unsigned char)190)
#define CONS_L0011 ((unsigned char)191)
#define CONS_L1100 ((unsigned char)192)
#define CONS_L1101 ((unsigned char)193)
#define CONS_L0111 ((unsigned char)194)
#define CONS_L1110 ((unsigned char)195)
#define CONS_L0101 ((unsigned char)196)
#define CONS_L1111 ((unsigned char)197)
#define CONS_L1210 ((unsigned char)198)
#define CONS_L2120 ((unsigned char)199)
#define CONS_L2200 ((unsigned char)200)
#define CONS_L0220 ((unsigned char)201)
#define CONS_L2202 ((unsigned char)202)
#define CONS_L0222 ((unsigned char)203)
#define CONS_L2220 ((unsigned char)204)
#define CONS_L0202 ((unsigned char)205)
#define CONS_L2222 ((unsigned char)206)
#define CONS_L1202 ((unsigned char)207)
#define CONS_L2101 ((unsigned char)208)
#define CONS_L0212 ((unsigned char)209)
#define CONS_L0121 ((unsigned char)210)
#define CONS_L2100 ((unsigned char)211)
#define CONS_L1200 ((unsigned char)212)
#define CONS_L0210 ((unsigned char)213)
#define CONS_L0120 ((unsigned char)214)
#define CONS_L2121 ((unsigned char)215)
#define CONS_L1212 ((unsigned char)216)
#define CONS_L1001 ((unsigned char)217)
#define CONS_L0110 ((unsigned char)218)
#define CONS_FBLOCK ((unsigned char)219)
#define CONS_LHBLOCK ((unsigned char)220)
#define CONS_LHBAR ((unsigned char)221)
#define CONS_RHBAR ((unsigned char)222)
#define CONS_HHBLOCK ((unsigned char)223)
#define CONS_MHBLOCK ((unsigned char)254)

console* console_new(const canvas*);
void console_delete(console*);
console_cell* console_ca(console*, unsigned x, unsigned y);
void console_puts(console*, const console_cell* template,
                  unsigned x0, unsigned y, const char*);
void console_putc(console*, const console_cell* template,
                  unsigned x, unsigned y, unsigned char);
void console_bel(console*);
void console_clear(console*);
void console_render(canvas*, console*);

#endif /* GRAPHICS_CONSOLE_H_ */
