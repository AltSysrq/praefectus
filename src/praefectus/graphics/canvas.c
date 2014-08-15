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

#include "../alloc.h"
#include "canvas.h"

canvas* canvas_new(unsigned short w, unsigned short h) {
  unsigned short pitch = (w + CANVAS_ALIGN - 1) / CANVAS_ALIGN * CANVAS_ALIGN;
  canvas* this;
  unsigned char* raw;

  this = xmalloc(sizeof(canvas) + pitch*h*sizeof(canvas_pixel) + CANVAS_ALIGN);
  this->w = w;
  this->h = h;
  this->pitch = pitch;

  raw = (unsigned char*)this;
  raw += sizeof(canvas);
  /* Using unsigned long will avoid a "cast from pointer to integer of
   * different size" warning on every modern platform but Win64. Since we only
   * care about the lowest 6 bits, it's safe even if long isn't sanely defined,
   * though.
   */
  this->data = raw + CANVAS_ALIGN - (((unsigned long)raw) % CANVAS_ALIGN);
  return this;
}
