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
#ifndef GRAPHICS_FONT_H_
#define GRAPHICS_FONT_H_

#include "bsd.h"

#include "canvas.h"

__BEGIN_DECLS

/**
 * @file
 *
 * Supports rendering characters using codepage 437 with the generic PC font.
 */

#define FONT_CHARW 9
#define FONT_CHARH 16

/**
 * Returns the number of horizontal pixels taken to render the given string.
 */
unsigned font_strwidth(const char*);
/**
 * Renders the given string onto the given canvas.
 *
 * @param lx The X coordinate of the upper left of the first character.
 * @param ly The Y coordinate of the upper left of the first character.
 */
void font_render(canvas*, signed lx, signed ty,
                 const char*, canvas_pixel);

/**
 * Renders the given character at the given coordinates.
 */
void font_renderch(canvas*, signed lx, signed ty, unsigned char, canvas_pixel);

__END_DECLS

#endif /* GRAPHICS_FONT_H_ */
