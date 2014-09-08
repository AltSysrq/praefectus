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
#include <assert.h>

#include "canvas.h"
#include "font.h"

typedef struct {
  const font_char* primary, * overstrike;
  canvas_pixel palette[4];
  signed x_off;
} font_glyph;

static unsigned font_chars_to_glyphs(
  font_glyph*, const compiled_font*, const char*,
  const canvas_pixel[4]);
static void font_position_glyphs(font_glyph*, unsigned);
static void font_rotpalette(canvas_pixel[4], const canvas_pixel[4],
                            unsigned);
static unsigned font_kmax(const unsigned[3]);
static void font_render_glyphs(canvas*, unsigned, unsigned,
                               const font_glyph*, unsigned, unsigned,
                               unsigned, char);
static void font_render_glyph(canvas*, unsigned, unsigned,
                              const font_char*, unsigned,
                              canvas_pixel, char);

static void font_infer_lead(unsigned* lead, font_char* ch,
                            unsigned y0, unsigned y1) {
  unsigned x, min = ~0, min2 = ~0, y;

  for (y = y0; y < y1; ++y) {
    x = 0;
    while (x < ch->pitch/2 && !strchr("-=#", ch->data[y*ch->pitch+x]))
      ++x;

    if (x < min) {
      min2 = min;
      min = x;
    } else if (x < min2) {
      min2 = x;
    }
  }

  *lead = min;
}

static void font_infer_tail(unsigned* tail, font_char* ch,
                            unsigned y0, unsigned y1) {
  unsigned x, max = 0, max2 = 0, y;

  for (y = y0; y < y1; ++y) {
    x = ch->pitch;
    while (x > ch->pitch/2 && !strchr("-=#", ch->data[y*ch->pitch+x-1]))
      --x;

    if (x > max) {
      max2 = max;
      max = x;
    } else if (x > max2) {
      max2 = x;
    }
  }

  *tail = max;
}

void font_compile(compiled_font* dst, const font_spec* spec) {
  unsigned i, x, y, len;
  signed ox, oy;
  font_char* ch;
  const font_char** table;
  char* px;
  unsigned char ix;

  dst->spec = spec;
  memset(dst->basic, 0, sizeof(dst->basic));
  memset(dst->reduced, 0, sizeof(dst->reduced));
  memset(dst->overstrike, 0, sizeof(dst->overstrike));

  for (ch = spec->chars; ch->name; ++ch) {
    len = strlen(ch->data);

    assert(0 == len % spec->rows);
    ch->pitch = len / spec->rows;

    for (px = ch->data; *px; ++px)
      assert(strchr(" -=#.", *px));

    /* Infer border if requested */
    if (!ch->manual_border) {
      for (y = 0; y < spec->rows; ++y) {
        for (x = 0; x < ch->pitch; ++x) {
          if (' ' == ch->data[y*ch->pitch+x]) {
            for (oy = -1; oy <= +1; ++oy) {
              if ((unsigned)(y+oy) < spec->rows) {
                for (ox = -1; ox <= +1; ++ox) {
                  if ((unsigned)(x+ox) < ch->pitch) {
                    if (strchr("-=#", ch->data[(y+oy)*ch->pitch+x+ox])) {
                      ch->data[y*ch->pitch+x] = '.';
                      goto next_char;
                    }
                  }
                }
              }
            }
          }

          next_char:;
        }
      }

      ch->manual_border = 1;
    }

    /* Infer kerning if requested */
    if (!ch->lead[0] && !ch->lead[1] && !ch->lead[2] &&
        !ch->tail[0] && !ch->tail[1] && !ch->tail[2]) {
      font_infer_lead(ch->lead+0, ch, 0, spec->baseline/2);
      font_infer_tail(ch->tail+0, ch, 0, spec->baseline/2);
      font_infer_lead(ch->lead+1, ch, spec->baseline/2, spec->baseline);
      font_infer_tail(ch->tail+1, ch, spec->baseline/2, spec->baseline);
      font_infer_lead(ch->lead+2, ch, spec->baseline, spec->rows);
      font_infer_tail(ch->tail+2, ch, spec->baseline, spec->rows);
    }

    switch (strlen(ch->name)) {
    default: abort();

    case 1:
      table = dst->basic;
      ix = ch->name[0];
      break;

    case 2:
      ix = ch->name[1];
      switch (ch->name[0]) {
      default: abort();

      case '^':
        table = dst->overstrike;
        break;

      case '_':
        table = dst->reduced;
        break;
      }
      break;
    }

    assert(ix < 128);
    table[ix] = ch;
  }

  dst->block.name = "block";
  dst->block.pitch = spec->rows / 2 + 1;
  dst->block.lead[0] = dst->block.lead[1] = dst->block.lead[2] = 0;
  dst->block.tail[0] = dst->block.tail[1] = dst->block.tail[2] =
    dst->block.pitch;
  dst->block.manual_border = 1;
  for (y = 0; y < spec->rows; ++y)
    for (x = 0; x < dst->block.pitch; ++x)
      dst->block.data[y*dst->block.pitch+x] =
        (y <= spec->baseline && x < dst->block.pitch-1?
         '#' : ' ');

  for (i = 0; i < 128; ++i) {
    if (!dst->basic[i])
      dst->basic[i] = &dst->block;
    if (!dst->overstrike[i])
      dst->overstrike[i] = dst->basic[i];
    if (!dst->reduced[i])
      dst->reduced[i] = dst->basic[i];
  }
}

#define DIGRAPH(a,b) (((unsigned)(a)) | (((unsigned)(b)) << 8))

static unsigned font_chars_to_glyphs(font_glyph* dst,
                                     const compiled_font* font,
                                     const char* str,
                                     const canvas_pixel init_palette[4]) {
  unsigned glyph_count = 0;
  canvas_pixel palette[4];
  unsigned char ch, overstrike;

  memcpy(palette, init_palette, sizeof(palette));

  while (*str) {
    overstrike = 0;
    switch (DIGRAPH(str[0], str[1])) {
    case DIGRAPH('f','l'): ch = *LIGATURE_fl; str += 2; break;
    case DIGRAPH('f','f'): ch = *LIGATURE_ff; str += 2; break;
    case DIGRAPH('f','i'): ch = *LIGATURE_fi; str += 2; break;
    case DIGRAPH('f','t'): ch = *LIGATURE_ft; str += 2; break;
    case DIGRAPH('A','E'):
    case DIGRAPH('A','e'): ch = *LIGATURE_AE; str += 2; break;
    case DIGRAPH('a','e'): ch = *LIGATURE_ae; str += 2; break;
    case DIGRAPH('O','E'):
    case DIGRAPH('O','e'): ch = *LIGATURE_OE; str += 2; break;
    case DIGRAPH('o','e'): ch = *LIGATURE_oe; str += 2; break;
    case DIGRAPH('t','z'): ch = *LIGATURE_tz; str += 2; break;

    case DIGRAPH('\a','^'):
      if (str[2] && str[3]) {
        overstrike = str[2];
        ch = str[3];
        str += 4;
      } else {
        ch = 0;
        str += 2;
      }
      break;

#define ROT(n) font_rotpalette(palette, init_palette, n)
    case DIGRAPH('\a','W'): ROT(0); str += 2; continue;
    case DIGRAPH('\a','R'): ROT(1); str += 2; continue;
    case DIGRAPH('\a','O'): ROT(2); str += 2; continue;
    case DIGRAPH('\a','Y'): ROT(3); str += 2; continue;
    case DIGRAPH('\a','G'): ROT(4); str += 2; continue;
    case DIGRAPH('\a','C'): ROT(5); str += 2; continue;
    case DIGRAPH('\a','B'): ROT(6); str += 2; continue;
    case DIGRAPH('\a','M'): ROT(7); str += 2; continue;
#undef ROT

    default:
      ch = *str++;
      break;
    }

    if (!overstrike) {
      dst->primary = font->basic[ch];
      dst->overstrike = NULL;
    } else {
      dst->primary = font->reduced[ch];
      dst->overstrike = font->overstrike[ch];
    }

    memcpy(dst->palette, palette, sizeof(palette));
    ++dst;
    ++glyph_count;
  }

  return glyph_count;
}

static void font_rotpalette(canvas_pixel dst[4],
                            const canvas_pixel src[4],
                            unsigned colours) {
  unsigned i;

  for (i = 0; i < 4; ++i)
    dst[i] = src[i] + colours*CP_SIZE;
}

static void font_position_glyphs(font_glyph* glyph, unsigned count) {
  unsigned i;
  signed xoff, prev_tail_kern[3] = { 0, 0, 0 };
  signed kern, max_kern;

  if (!count) return;

  xoff = 0;
  for (; count; ++glyph, --count) {
    max_kern = -65536;
    for (i = 0; i < 3; ++i) {
      kern = prev_tail_kern[i] - glyph->primary->lead[i];
      if (kern > max_kern)
        max_kern = kern;

      prev_tail_kern[i] = glyph->primary->tail[i] - glyph->primary->pitch;
    }

    xoff += max_kern + 1;
    glyph->x_off = xoff;
    xoff += glyph->primary->pitch;
  }
}

static unsigned font_kmax(const unsigned kern[3]) {
  unsigned max = 0, i;

  for (i = 0; i < 3; ++i)
    if (kern[i] > max)
      max = kern[i];

  return max;
}

unsigned font_strwidth(const compiled_font* font, const char* str) {
  static const canvas_pixel palette[4] = { 0,0,0,0 };
  font_glyph glyphs[strlen(str)];
  unsigned num_glyphs;

  num_glyphs = font_chars_to_glyphs(glyphs, font, str, palette);
  font_position_glyphs(glyphs, num_glyphs);

  if (!num_glyphs) return 0;

  return font_kmax(glyphs[num_glyphs-1].primary->tail) +
    glyphs[num_glyphs-1].x_off;
}

void font_render(canvas* dst, const compiled_font* font,
                 const char* str, unsigned x0, unsigned y0,
                 const canvas_pixel palette[4],
                 int show_border) {
  font_glyph glyphs[strlen(str)];
  unsigned num_glyphs;
  unsigned rows = font->spec->rows;

  num_glyphs = font_chars_to_glyphs(glyphs, font, str, palette);
  font_position_glyphs(glyphs, num_glyphs);

  if (show_border)
    font_render_glyphs(dst, x0, y0, glyphs, num_glyphs, rows, 3, '.');
  font_render_glyphs(dst, x0, y0, glyphs, num_glyphs, rows, 2, '-');
  font_render_glyphs(dst, x0, y0, glyphs, num_glyphs, rows, 1, '=');
  font_render_glyphs(dst, x0, y0, glyphs, num_glyphs, rows, 0, '#');
}

static void font_render_glyphs(canvas* dst, unsigned x0, unsigned y0,
                               const font_glyph* glyphs, unsigned num_glyphs,
                               unsigned rows,
                               unsigned colour_ix, char ch) {
  unsigned i;

  for (i = 0; i < num_glyphs; ++i) {
    font_render_glyph(dst, x0 + glyphs[i].x_off, y0,
                      glyphs[i].primary, rows,
                      glyphs[i].palette[colour_ix], ch);
    if (glyphs[i].overstrike)
      font_render_glyph(
        dst,
        x0 + glyphs[i].x_off -
          /* Assume overstrike and primary are both overall centred within their
           * pitch.
           */
          glyphs[i].overstrike->pitch/2 +
          glyphs[i].primary->pitch/2,
        y0, glyphs[i].overstrike, rows,
        glyphs[i].palette[colour_ix], ch);
  }
}

static void font_render_glyph(canvas* dst, unsigned x0, unsigned y0,
                              const font_char* glyph, unsigned rows,
                              canvas_pixel px, char ch) {
  unsigned x, y;

  for (y = 0; y < rows; ++y)
    if (y+y0 < dst->h)
      for (x = 0; x < glyph->pitch; ++x)
        if (x+x0 < dst->w)
          if (ch == glyph->data[y*glyph->pitch + x])
            dst->data[canvas_off(dst, x+x0, y+y0)] = px;
}
