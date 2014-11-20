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
#ifndef GRAPHICS_CANVAS_H_
#define GRAPHICS_CANVAS_H_

#include "bsd.h"

__BEGIN_DECLS

/**
 * @file
 *
 * Praefectus uses an 8-bit graphical style. Instead of this just meaning
 * low-resolution textures, we take it a LOT further here.
 *
 * The game renders to a virtual screen 480 pixels tall (width based upon
 * window width in order to get square pixels), using an 8-bit colour
 * palette. This video buffer is mapped into 24-bit RGB (though at only 6 bits
 * per pixel) and sent through a simulated degraded VGA cable, introducing low
 * levels of white noise and continuous ghosting (I'm not sure what this effect
 * is normally called) and projected onto a CRT with the same resolution. The
 * CRT has colour components in the "triangular" RGBRGB.../GBRGBR.../BRGBRG...
 * pattern; colour components can only reduce their brightness by at most 50%
 * on each refresh. This CRT screen is projected onto a paraboloid, which is
 * then projected onto the actual window, such that the mid-points of each CRT
 * edge correspond to the same mid-points of the window. Colour components
 * bleed into their immediate area, which allows a high-resolution LCD to
 * approximate the appearance of a low-resolution CRT, while allowing
 * lower-resolution windows to still be comprehensible.
 *
 * The colour palette is divided into 8 planes. The zeroth plane is greyscale,
 * and contains 32 elements ranging from black to white. The other planes each
 * contain 32 shades of red, orange, yellow, green, cyan, blue, and violet,
 * respectively.
 */

/**
 * Data type for canvas pixels. This value indexes the colour palette in use
 * when projected to the screen.
 */
typedef unsigned char canvas_pixel;

/**
 * The guaranteed memory alignment of each row in a canvas.
 */
#define CANVAS_ALIGN 64

/**
 * The black..grey..white colour plane.
 */
#define CP_GREY         0x00
#define CP_RED          0x20
#define CP_ORANGE       0x40
#define CP_YELLOW       0x60
#define CP_GREEN        0x80
#define CP_CYAN         0xA0
#define CP_BLUE         0xC0
#define CP_VIOLET       0xE0
/**
 * The number of indices within one colour plane.
 */
#define CP_SIZE         0x20
/**
 * The offset within a colour plane of the most intense colour. For example,
 * CP_GREY+CP_MAX is pure white in the normal palette, and CP_RED+CP_MAX is
 * 0xFF0000 in the normal palette.
 */
#define CP_MAX          (CP_SIZE-1)

/**
 * Virtually all drawing operations render to a canvas. A canvas is essentially
 * a two-dimensional array of canvas_pixels.
 */
typedef struct {
  /**
   * The number of meaningful pixels in a horizontal scan-line of the canvas.
   */
  unsigned short w;
  /**
   * The number of rows in the canvas.
   */
  unsigned short h;
  /**
   * The width in bytes of a single row. This is often greater than w for
   * alignment purposes.
   */
  unsigned short pitch;
  /**
   * The data for this canvas. Its usable size is equal to (pitch*h), though
   * note that the bytes beyond (w) for each row are meaningless. For any N,
   * (data+N*pitch) is guaranteed to have 64-byte alignment.
   *
   * This is in row-major order. Pixels run left-to-right, top-to-bottom.
   */
  canvas_pixel* data;
} canvas;

/**
 * Allocates a new canvas of the given logical dimensions. The canvas and its
 * data can be freed with a single free() call.
 */
canvas* canvas_new(unsigned short w, unsigned short h);

/**
 * Returns the byte offset in the given canvas for the given x,y coordinates.
 */
static inline unsigned canvas_off(const canvas* c, unsigned x, unsigned y) {
  return y * c->pitch + x;
}

static inline void canvas_put(canvas* dst, signed short x, signed short y,
                              canvas_pixel px) {
  if (x >= 0 && x < dst->w && y >= 0 && y < dst->h)
    dst->data[canvas_off(dst, x, y)] = px;
}

__END_DECLS

#endif /* GRAPHICS_CANVAS_H_ */
