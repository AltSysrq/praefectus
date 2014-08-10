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

/**
 * @file
 *
 * Praefectus uses an 8-bit graphical style. Instead of this just meaning
 * low-resolution textures, we take it a LOT further here.
 *
 * The game renders to a virtual screen 480 pixels tall (width based upon
 * window width in order to get square pixels), using an 8-bit colour
 * palette. This video buffer is mapped into 24-bit RGB (though at only 7 bits
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

typedef unsigned char canvas_pixel;

#define CP_GREY         0x00
#define CP_RED          0x20
#define CP_ORANGE       0x40
#define CP_GREEN        0x60
#define CP_CYAN         0x80
#define CP_BLUE         0xA0
#define CP_VIOLET       0xC0
#define CP_SIZE         0x20
#define CP_MAX          (CP_SIZE-1)

typedef struct {
  unsigned short w, h, pitch;
  canvas_pixel data[FLEXIBLE_ARRAY_MEMBER];
} canvas;

canvas* canvas_new(unsigned short w, unsigned short h);

#endif /* GRAPHICS_CANVAS_H_ */
