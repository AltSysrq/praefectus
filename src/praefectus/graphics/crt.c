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
#include "../frac.h"
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
 * parabolically into (dst_x,dst_y), which are 16.8 fixed-point coordinates
 * into the screen.
 */
static inline void crt_project_parabolic(const crt_screen* crt,
                                         signed*restrict dst_x, signed*restrict dst_y,
                                         signed x, signed y,
                                         fraction iw, fraction ih,
                                         unsigned cx, unsigned cy,
                                         fraction icx2, fraction icy2) {
  signed ox = x - cx, oy = y - cy;
  unsigned fx = fraction_fpmul(ox*ox, icx2, 16);
  unsigned fy = fraction_fpmul(oy*oy, icy2, 16);
  unsigned f = fx + fy;
  signed factor;

  factor = BASE_FACTOR * 256LL + BULGE_FACTOR * f / 256LL;

  *dst_x = fraction_smul(crt->w * (cx * 65536LL + ox * factor) / 256, iw);
  *dst_y = fraction_smul(crt->h * (cy * 65536LL + oy * factor) / 256, ih);
}

static inline crt_colour crt_sample(const crt_screen* crt,
                                    signed x, signed y) {
  x >>= 8;
  y >>= 8;

  if (x < 0 || x >= crt->w || y < 0 || y >= crt->h) return 0;

  return crt->data[crt_screen_off(crt, x, y)];
}

static inline void put_px(unsigned*restrict dst, unsigned dx, unsigned dy,
                          unsigned dpitch,
                          const crt_screen* src, unsigned sx, unsigned sy) {
  dst[dy*dpitch + dx] = crt_sample(src, sx, sy) << 2;
}

void crt_screen_proj(unsigned*restrict dst, unsigned dw, unsigned dh,
                     unsigned dpitch, const crt_screen* src) {
  fraction iw = fraction_of(dw), ih = fraction_of(dh);
  unsigned cx = dw/2, cy = dh/2;
  fraction icx = fraction_of(cx), icx2 = fraction_umul(icx, icx);
  fraction icy = fraction_of(cy), icy2 = fraction_umul(icy, icy);

  unsigned x, y;
  signed px, py;

  /* TODO: bleed/glow/scanlines */

  /* Only iterate over the upper quarter of the screen (including the centre
   * lines in the extremely rare case of an odd resolution), since the other
   * quarters are symmetric. This saves us 75% of the projection calculations.
   */
  for (y = 0; y < (dh+1)/2; ++y) {
    for (x = 0; x < (dw+1)/2; ++x) {
      crt_project_parabolic(src, &px, &py, x, y,
                            iw, ih, cx, cy, icx2, icy2);
      put_px(dst, x,      y,      dpitch, src, px,                py);
      put_px(dst, dw-x-1, y,      dpitch, src, src->w*256-px-256, py);
      put_px(dst, x,      dh-y-1, dpitch, src, px,                src->h*256-py-256);
      put_px(dst, dw-x-1, dh-y-1, dpitch, src, src->w*256-px-256, src->h*256-py-256);
    }
  }
}
