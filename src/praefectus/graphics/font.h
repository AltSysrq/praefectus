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

#include "canvas.h"

#define LIGATURE_fl "\1"
#define LIGATURE_ff "\2"
#define LIGATURE_fi "\3"
#define LIGATURE_ft "\4"
#define LIGATURE_AE "\5"
#define LIGATURE_ae "\6"
#define LIGATURE_OE "\7"
#define LIGATURE_oe "\10"
#define LIGATURE_tz "\11"

/**
 * @file
 *
 * In order to match the low-resolution style of the rest of the game,
 * praefectus uses hand-defined raster fonts. These fonts only support the
 * ASCII character set, but there is support for overstriking characters
 * sufficient to support the silly Polish names of most of the in-game
 * objects.
 *
 * Every character is defined by a rectangular grid of characters from the set
 * ' ', '-', '=', '#', and '.'. ' ' is always transparent. '#', '=', and '-'
 * correspond to palette colours 0, 1, and 2, respetively. Generally colour 1
 * is full intensity, 1 is half intensity, and 2 is quarter intensity. '.'
 * corresponds to palette colour 3, but is transparent unless the font is drawn
 * with borders enabled. Palettes are specified as indices into the the canvas
 * palette when a string is rendered.
 *
 * Characters are measured as per the diagramme below. Note that kerning is
 * automatically derived from lead/tail pairs of adjacent characters.
 *
 *             /-- Lead mid, bottom
 *             |/-- Lead top
 *             || /-- Tail top
 *             || |/-- Tail bottom
 *             || ||/-- Tail mid
 *             vv vvv
 * origin ->/ "      " \
 *          | "      " |
 *          | " #### " | em
 *          | "#   # " |
 *     rows | "#  ## " |
 *          | " ## # " / <- baseline
 *          | "#  #  "
 *          \ " ##   "
 *             \----/
 *             pitch
 *
 * Leads generally refer to the first non-border non-transparent pixel at that
 * hight, whereas tail refers one past the last such pixel.
 *
 * Each character has up to three forms. The basic form is usually used; the
 * other forms default to the default form if none is present. When one
 * character is overstricken another, the former uses the overstrike form, and
 * the latter uses the reduced form. This is necessary, for example, to form Ż
 * (latin capital Z with dot above), which is represented as a Z with an
 * overstricken '.', requires the '.' to be moved to the top, and the Z to be
 * shortened so that the dot is visible.
 *
 * Overstrike forms of characters are assumed to be centred, and will be
 * centred on top of the underlying character. Since we only care about
 * supporting Polish diacritics, this is sufficient.
 *
 * Ligatures exist for the letter pairs "fl", "ff", "fi", "ft", "AE", "ae",
 * "OE", "oe", and "tz". "Ae" and "Oe" are mapped to "AE" and "OE". Ligatures
 * are mapped to characters in the control character region of ASCII.
 *
 * Font rendering can be affected by escape sequences. Escape sequences begin
 * with BEL ('\a') followed by one character indicating the desired effect, as
 * per the following table. It is always safe to render arbitrary untrusted
 * text.
 *
 *   \a^        The following two characters will be overstricken. Overstrike
 *              character comes first, followed by reduced character.
 *              Example: "je\a^.zy\a^/la"
 *
 *   \aROYGCBM  The colour palette will be rotated from the input palette such
 *              that if the input palette where white, the new palette would be
 *              red, orange, yellow, green, cyan, blue, or magenta,
 *              respectively.
 *
 *   \aW        The colour palette is reset to the input palette.
 *
 * When rendering, pixels are drawn grouped by palette index in descending
 * order, so that more intense pixels are always on top.
 *
 * For most characters, border and lead/tail can be calculated
 * automatically. This is signified by all leads/tails being zero and the
 * manual_border field being false, respeectively. This is generally indicated
 * by omitting these fields from the value definition.
 */

/**
 * The basic representation of a single character for a font. Note that this is
 * generally mutated in-place when the font is first compiled.
 */
typedef struct {
  /**
   * The name of this character. For basic forms, this is a single-character
   * string consisting of the character itself, or one of the LIGATURE_*
   * constants. Overstrike forms are prefixed with '^'; reduced forms are
   * prefixed with '_'.
   *
   * A NULL name indicates the end of a character array.
   */
  const char* name;
  /**
   * The data for this chacter, consisting of the 5 meaningful characters. It
   * is assumed to have the same number of rows as the rest of the font. The
   * pitch is inferred by dividing the length of this string by the height of
   * the font, and it must divide evenly.
   */
  char data[256];
  /**
   * The offsets of the leads and tails, respectively. If all 6 are zero, the
   * leads and tails will be inferred when the font will be compiled.
   */
  unsigned lead[3], tail[3];
  /**
   * If true, border pixels in the font will not be inferred when the font is
   * compiled.
   */
  int manual_border;
  /**
   * The number of pixels in each row of this character. This is inferred when
   * the font is compiled, so there is never any need to specify it.
   */
  unsigned pitch;
} font_char;

typedef struct {
  /**
   * The number of physical pixels high each character in the font is.
   */
  unsigned rows;
  /**
   * The offset from the top of one row to the logical "baseline" as it would
   * be defined by the Latin script.
   */
  unsigned baseline;
  /**
   * The offset from the top of the one row of characters to one row of the
   * next line.
   */
  unsigned em;
  /**
   * An array of characters belonging to this font. The final value is
   * indicated by a font_char with a NULL name.
   */
  font_char* chars;
} font_spec;

typedef struct {
  /**
   * The base spec for this font.
   */
  const font_spec* spec;
  /**
   * Maps from ASCII character codes to character forms. No entries are
   * NULL. reduced and overstrike forms do have references to basic-form
   * characters if those characters do not have special alternate forms.
   */
  const font_char* basic[128], * reduced[128], * overstrike[128];

  /**
   * A basic block-shaped character used for characters not defined by the
   * font.
   */
  font_char block;
} compiled_font;

/**
 * Compiles the given font. If the font is invalid, the program aborts.
 *
 * This call has the side-effect of setting all automatic fields on any
 * characters referenced by the font.
 */
void font_compile(compiled_font*, const font_spec*);
/**
 * Returns the width, in pixels, of the given string. The earliest lead of the
 * initial character and the latest tail of the final character are used for
 * this calculation, so the result is pessimistic.
 */
unsigned font_strwidth(const compiled_font*, const char*);
/**
 * Renders the given string to the canvas at selected coordinates. The font
 * need not be in-bounds.
 *
 * @param dst The canvas to which to render.
 * @param font The font to use for rendering.
 * @param txt The raw string to render.
 * @param x The x coordinate for the upper-left origin of the first
 * character. The earliest lead of the first character is used to position the
 * string.
 * @param y The y coordinate for the upper-left origin of the first
 * character.
 * @param init_palette The four colour indices to map to the font pixels. The
 * third entry is only meaningful if show_border is true. Note that escape
 * sequences can rotate the palette.
 * @param show_border Whether pixels with value 3 will be shown.
 */
void font_render(canvas* dst, const compiled_font* font,
                 const char* txt, unsigned x, unsigned y,
                 const canvas_pixel init_palette[4],
                 int show_border);

#endif /* GRAPHICS_FONT_H_ */
