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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <bsd.h>

#define MAX_NODES 256
#define NUM_PX 1024

typedef struct {
  unsigned short x, y;
} position;

typedef struct {
  unsigned instant, self_node_id, other_node_id;
  unsigned x, y;
  signed vx, vy;
  unsigned latency;
} input_line;

static unsigned node_ids[MAX_NODES];
static const Uint32 colours[] = {
  0xFF0000,
  0xFFFF00,
  0x00FF00,
  0x00FFFF,
  0x0000FF,
  0xFF00FF,
  0xFFFFFF,
  0xAAAAAA,
};

static unsigned index_of_node(unsigned);
static int read_line(input_line*, FILE*);
static void prescan_input(
  unsigned* num_instants,
  unsigned* num_nodes,
  FILE* input);
static void read_input(
  unsigned num_instants,
  unsigned num_nodes,
  position dst[num_instants][num_nodes][num_nodes],
  FILE* input);
static void draw_instant(
  SDL_Renderer*,
  unsigned num_nodes,
  const position instant0[num_nodes][num_nodes],
  const position instant1[num_nodes][num_nodes]);
static void run(
  SDL_Renderer*,
  unsigned num_instants,
  unsigned num_nodes,
  const position data[num_instants][num_nodes][num_nodes]);

int main(int argc, char** argv) {
  FILE* input;
  void* data;
  unsigned num_instants, num_nodes;
  SDL_Window* screen;
  SDL_Renderer* renderer;

  if (2 != argc)
    errx(EX_USAGE, "Usage: %s <infile>", argv[0]);

  if (SDL_Init(SDL_INIT_VIDEO))
    errx(EX_SOFTWARE, "Unable to initialise SDL: %s", SDL_GetError());

  atexit(SDL_Quit);

  screen = SDL_CreateWindow("NBodies Visualiser",
                            0, 0,
                            NUM_PX, NUM_PX,
                            0);
  if (!screen)
    errx(EX_OSERR, "Unable to create window: %s", SDL_GetError());

  renderer = SDL_CreateRenderer(screen, -1, 0);
  if (!renderer)
    errx(EX_SOFTWARE, "Unable to create SDL renderer: %s",
         SDL_GetError());

  input = fopen(argv[1], "r");
  if (!input)
    err(EX_NOINPUT, "Failed to open %s", argv[1]);

  prescan_input(&num_instants, &num_nodes, input);
  rewind(input);

  data = calloc(num_instants * num_nodes * num_nodes, sizeof(position));
  if (!data)
    errx(EX_UNAVAILABLE, "Failed to allocate memory");

  read_input(num_instants, num_nodes, data, input);
  fclose(input);

  run(renderer, num_instants, num_nodes, data);
  return 0;
}

static int read_line(input_line* dst, FILE* from) {
  int count =
    fscanf(from, "%d,%X,%X,%X,%X,%d,%d,%d\n",
           &dst->instant,
           &dst->self_node_id, &dst->other_node_id,
           &dst->x, &dst->y, &dst->vx, &dst->vy,
           &dst->latency);

  if (-1 == count) return 0;
  if (8 == count) return 1;

  errx(EX_DATAERR, "Malformed input file");
}

static unsigned index_of_node(unsigned node_id) {
  unsigned i;

  for (i = 0; node_ids[i] && i < MAX_NODES; ++i)
    if (node_id == node_ids[i])
      return i;

  if (i >= MAX_NODES)
    errx(EX_DATAERR, "Maximum node count exceeded");

  node_ids[i] = node_id;
  return i;
}

static void prescan_input(unsigned* num_instants, unsigned* num_nodes,
                          FILE* input) {
  input_line line;

  *num_instants = 0;
  while (read_line(&line, input)) {
    index_of_node(line.self_node_id);
    index_of_node(line.other_node_id);
    if (line.instant >= *num_instants)
      *num_instants = line.instant + 1;
  }

  for (*num_nodes = 0; node_ids[*num_nodes]; ++*num_nodes);
}

static void read_input(unsigned num_instants,
                       unsigned num_nodes,
                       position dst[num_instants][num_nodes][num_nodes],
                       FILE* input) {
  input_line line;
  unsigned oix, six;

  while (read_line(&line, input)) {
    six = index_of_node(line.self_node_id);
    oix = index_of_node(line.other_node_id);
    if (line.instant >= num_instants ||
        six >= num_nodes ||
        oix >= num_nodes)
      errx(EX_DATAERR, "File apparently changed after first pass");

    dst[line.instant][oix][six].x = line.x / 65536;
    dst[line.instant][oix][six].y = line.y / 65536;
  }
}

static void draw_instant(SDL_Renderer* renderer,
                         unsigned num_nodes,
                         const position p0[num_nodes][num_nodes],
                         const position p1[num_nodes][num_nodes]) {
  unsigned n0, n1, c;

  for (n0 = 0; n0 < num_nodes; ++n0) {
    c = n0 % (sizeof(colours) / sizeof(colours[0]));
    SDL_SetRenderDrawColor(
      renderer,
      (colours[c] >> 16) & 0xFF,
      (colours[c] >>  8) & 0xFF,
      (colours[c] >>  0) & 0xFF,
      SDL_ALPHA_OPAQUE);

    for (n1 = 0; n1 < num_nodes; ++n1) {
      SDL_RenderDrawLine(renderer, p0[n0][n1].x, p0[n0][n1].y,
                         p1[n0][n1].x, p1[n0][n1].y);
    }
  }
}

static void run(SDL_Renderer* renderer,
                unsigned num_instants,
                unsigned num_nodes,
                const position data[num_instants][num_nodes][num_nodes]) {
  int alive = 1, auto_advance = 0;
  unsigned last_instant = 1, tail_length = 8;
  unsigned i;
  SDL_Event evt;

  while (alive) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    for (i = last_instant; i > last_instant - tail_length && (i-1) < num_instants;
         --i) {
      draw_instant(renderer, num_nodes, data[i], data[i-1]);
    }
    SDL_RenderPresent(renderer);

    SDL_Delay(20);

    if (auto_advance && last_instant+1 < num_instants)
      ++last_instant;

    while (SDL_PollEvent(&evt)) {
      switch (evt.type) {
      case SDL_QUIT: alive = 0; break;
      case SDL_KEYDOWN:
        switch (evt.key.keysym.sym) {
        case SDLK_ESCAPE: alive = 0; break;
        case SDLK_SPACE: auto_advance = !auto_advance; break;
        case SDLK_F1: if (last_instant >= 1) --last_instant; break;
        case SDLK_F2: if (last_instant+1 < num_instants) ++last_instant; break;
        case SDLK_F3: if (tail_length >= 1) --tail_length; break;
        case SDLK_F4: ++tail_length; break;
        }
        break;
      }
    }
  }
}
