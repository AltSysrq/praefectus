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
#ifndef GRAPHICS_CRT_H_
#define GRAPHICS_CRT_H_

#include <SDL.h>

#include "bsd.h"

#include "canvas.h"

__BEGIN_DECLS

/**
 * Stores the state for the intermediate simulated-CRT projection.
 *
 * See the description in canvas.h for what the CRT projection does.
 */
typedef struct crt_screen_s crt_screen;

/**
 * Describes a colour for the CRT. Used to transform canvas_pixels into RGB for
 * display.
 *
 * Note that only the lower 6 bits of each colour component may be used. Thus,
 * the bit layout for these values is
 *
 *   00000000.00RRRRRR.00GGGGGG.00BBBBBB
 */
typedef unsigned crt_colour;

/**
 * Allocates a new, black CRT screen of the given dimensions.
 *
 * @param w The logical width of the CRT.
 * @param w The logical height of the CRT.
 * @param dw The width of the framebuffer to which the CRT will be projected.
 * @param dh The height of the framebuffer to which the CRT will be projected.
 * @param dpitch The pitch of the framebuffer to which the CRT will be
 * projected.
 */
crt_screen* crt_screen_new(unsigned short w, unsigned short h,
                           unsigned dw, unsigned dh, unsigned dpitch);
/**
 * Frees the memory held by the given crt_screen.
 */
void crt_screen_delete(crt_screen*);

/**
 * Transfers pixels from the given canvas onto the given CRT, through the
 * simulated VGA cable, after mapping via the provided colour palette.
 *
 * @param dst The target screen.
 * @param src The source canvas.
 * @param palette The lookup table from canvas_pixel values to
 * crt_colours. Assumed to be of length 256.
 */
void crt_screen_xfer(crt_screen* dst, const canvas*restrict src,
                     const crt_colour*restrict palette);

/**
 * Projects the current state of the CRT screen onto the given pixel array,
 * assumed to be in ARGB8888 format. It is assumed that the two have
 * approximately the same aspect ratio.
 */
void crt_screen_proj(unsigned*restrict dst,
                     const crt_screen* src);

/**
 * Populates the given palette with the default colours.
 */
void crt_default_palette(crt_colour*);

__END_DECLS

#endif /* GRAPHICS_CRT_H_ */
