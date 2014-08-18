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

#include <string.h>
#include <stdlib.h>

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

static inline crt_colour maxcomp(crt_colour a, crt_colour b,
                                 crt_colour mask) {
  if ((a & mask) > (b & mask))
    return a & mask;
  else
    return b & mask;
}

static inline crt_colour addc_clamped(crt_colour a, crt_colour b,
                                      unsigned maxmask) {
  if ((a & maxmask) + (b & maxmask) > maxmask) return maxmask;
  else return (a & maxmask) + (b & maxmask);
}

static inline crt_colour add_clamped(crt_colour a, crt_colour b) {
  return
    addc_clamped(a, b, 0x003F0000) |
    addc_clamped(a, b, 0x00003F00) |
    addc_clamped(a, b, 0x0000003F);
}

static inline unsigned seepage(crt_colour c) {
  unsigned r, g, b;

  r = (c >>  0) & 0x3F;
  g = (c >> 16) & 0x3F;
  b = (c >> 24) & 0x3F;

  return (r + g/4 + b/4)*(g + r/4 + b/4)*(b + r/4 + b/4);
}

void crt_screen_xfer(crt_screen* dst, const canvas*restrict src,
                     const crt_colour*restrict palette) {
  unsigned charge_decay = 0x40000 / dst->w;
  unsigned x, y, noise, charge, ghosting;
  crt_colour old, new;

  for (y = 0; y < dst->h; ++y) {
    charge = 0;

    for (x = 0; x < dst->w; ++x) {
      old = dst->data[crt_screen_off(dst, x, y)];
      old = (old >> 1) & 0x003F3F3F;
      new = palette[src->data[canvas_off(src, x, y)]];

      ghosting = (charge >> 16) & 0x3F;
      charge += seepage(new);
      if (charge > charge_decay)
        charge -= charge_decay;
      else
        charge = 0;
      new = add_clamped(new, (ghosting << 16) | (ghosting << 8) | ghosting);

      noise = rand() & 0x3;
      noise = (noise << 16) | (noise << 8) | noise;
      new = add_clamped(new, noise);

      dst->data[crt_screen_off(dst, x, y)] =
        maxcomp(old, new, 0x00FF0000) |
        maxcomp(old, new, 0x0000FF00) |
        maxcomp(old, new, 0x000000FF);
    }
  }
}

#define BULGE_FACTOR 24LL
#define BASE_FACTOR (256LL - BULGE_FACTOR)

/**
 * Projects the given coordinates (x,y), whose maximum values are (w,h),
 * parabolically into (dst_x,dst_y), which are 16.16 fixed-point coordinates
 * into the screen.
 */
static inline void crt_project_parabolic(const crt_screen* crt,
                                         signed* dst_x, signed* dst_y,
                                         signed x, signed y,
                                         signed long long w,
                                         signed long long h) {
  signed cx = w/2, cy = h/2;
  signed ox = x - cx, oy = y - cy;
  signed long long fx = abs(ox)*abs(ox) * 65536LL / cx / cx;
  signed long long fy = abs(oy)*abs(oy) * 65536LL / cy / cy;
  signed long long f = fx + fy;
  signed factor;

  factor = BASE_FACTOR + BULGE_FACTOR * f / 65536LL;

  *dst_x = crt->w * 256LL * (cx * 256LL + ox * factor) / w;
  *dst_y = crt->h * 256LL * (cy * 256LL + oy * factor) / h;
}

static inline crt_colour crt_sample(const crt_screen* crt,
                                    signed x, signed y) {
  x >>= 16;
  y >>= 16;

  if (x < 0 || x >= crt->w || y < 0 || y >= crt->h) return 0;

  return crt->data[crt_screen_off(crt, x, y)];
}

void crt_screen_proj(unsigned* dst, unsigned dw, unsigned dh, unsigned dpitch,
                     const crt_screen* src) {
  unsigned x, y;
  signed px, py;

  /* TODO: Actual projection / bleed */
  for (y = 0; y < dh; ++y) {
    for (x = 0; x < dw; ++x) {
      crt_project_parabolic(src, &px, &py, x, y, dw, dh);
      dst[y*dpitch + x] = crt_sample(src, px, py) << 2;
    }
  }
}