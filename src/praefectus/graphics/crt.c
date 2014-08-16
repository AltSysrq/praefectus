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
#include "canvas.h"
#include "crt.h"

struct crt_screen_s {
  unsigned short w, h;
  crt_colour data[FLEXIBLE_ARRAY_MEMBER];
};

static inline unsigned crt_screen_off(
  const crt_screen*restrict crt, unsigned x, unsigned y
) {
  return y * crt->w + x;
}

crt_screen* crt_screen_new(unsigned short w, unsigned short h) {
  crt_screen* this = xmalloc(offsetof(crt_screen, data) +
                             sizeof(crt_colour) * w * h);

  this->w = w;
  this->h = h;
  memset(this->data, 0, sizeof(crt_colour) * w * h);

  return this;
}

void crt_screen_delete(crt_screen* this) {
  free(this);
}

void crt_screen_xfer(crt_screen* dst, const canvas*restrict src,
                     const crt_colour*restrict palette) {
  unsigned x, y;

  /* TODO: VGA, fade effects */
  for (y = 0; y < dst->h; ++y)
    for (x = 0; x < dst->w; ++x)
      dst->data[crt_screen_off(dst, x, y)] =
        palette[src->data[canvas_off(src, x, y)]];
}

void crt_screen_proj(unsigned* dst, unsigned dw, unsigned dh, unsigned dpitch,
                     const crt_screen* src) {
  unsigned x, y;

  /* TODO: Actual projection / bleed */
  for (y = 0; y < dh; ++y)
    for (x = 0; x < dw; ++x)
      dst[y*dpitch + x] =
        src->data[crt_screen_off(src, x * src->w / dw, y * src->h / dh)]
        << 2;
}
