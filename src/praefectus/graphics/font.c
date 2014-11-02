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
#include <stdlib.h>

#include "canvas.h"
#include "font.h"

#include "437.xpm"

#define XPM_ROW_OFFSET 3
#define XPM_PITCH_CHARS 32

unsigned font_strwidth(const char* str) {
  return FONT_CHARW * strlen(str);
}

void font_render(canvas* dst, signed x0, signed y0,
                 const char* str, canvas_pixel colour) {
  while (*str) {
    font_renderch(dst, x0, y0, *str, colour);
    ++str;
    x0 += FONT_CHARW;
  }
}

void font_renderch(canvas* dst, signed cx0, signed y0,
                   unsigned char c, canvas_pixel colour) {
  signed ox, oy;
  for (oy = 0; oy < FONT_CHARH; ++oy) {
    if (y0+oy >= 0 && y0+oy < dst->h) {
      for (ox = 0; ox < FONT_CHARW; ++ox) {
        if (cx0+ox >= 0 && cx0+ox < dst->w) {
          if (' ' != f437_xpm[c / XPM_PITCH_CHARS * FONT_CHARH + oy + XPM_ROW_OFFSET]
                             [c % XPM_PITCH_CHARS * FONT_CHARW + ox])
            dst->data[canvas_off(dst, cx0+ox, y0+oy)] = colour;
        }
      }
    }
  }
}
