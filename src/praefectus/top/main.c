/*-
 * Copyright (c) 2013, 2014 Jason Lingle
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

/* Need to define _GNU_SOURCE on GNU systems to get RTLD_NEXT. BSD gives it to
 * us for free.
 */
#ifndef RTLD_NEXT
#define _GNU_SOURCE
#endif

#include <SDL.h>
#include <SDL_image.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#if defined(RTLD_NEXT) && defined(HAVE_DLFCN_H) && defined(HAVE_DLFUNC)
#define ENABLE_SDL_ZAPHOD_MODE_FIX
#endif

#include <libpraefectus/common.h>

#include "bsd.h"
#include "../game-state.h"
#include "../alloc.h"
#include "test-state.h"

/* Move this somewhere else if anything else winds up needing it. */
SDL_PixelFormat* screen_pixel_format;
#define SECOND 64

/* Whether the multi-display configuration might be a Zaphod configuration. If
 * this is true, we need to try to force SDL to respect the environment and
 * choose the correct display. Otherwise, allow SDL to decide on its own.
 *
 * (We can't just assume that the format :%d.%d indicates Zaphod, as Xinerama
 * displays often use :0.0 for the assembly of all the screens.)
 */
static int might_be_zaphod = 1;

static game_state* update(game_state*);
static void draw(canvas*, crt_screen*, game_state*, SDL_Window*);
static int handle_input(game_state*);

static int parse_x11_screen(signed* screen, const char* display) {
  if (!display) return 0;

  if (':' == display[0])
    return 1 == sscanf(display, ":%*d.%d", screen);
  else
    return 1 == sscanf(display, "%*64s:%*d.%d", screen);
}

#ifdef ENABLE_SDL_ZAPHOD_MODE_FIX
/* On X11 with "Zaphod mode" multiple displays, all displays have a position of
 * (0,0), so SDL always uses the same screen regardless of $DISPLAY. Work
 * around this overriding the internal (!!) function it uses to select display
 * screens, and providing the correct response for Zaphod mode ourselves. If
 * not in Zaphod mode, delegate to the original function.
 */
int SDL_GetWindowDisplayIndex(SDL_Window* window) {
  const char* display_name;
  /* We can consider this thread-safe. SDL only allows window manipulation by
   * the thread that creates the window, and we only create one window. (And if
   * we were to create more, they'd also be owned by the main thread anyway.)
   *
   * It is worth caching this, since SDL calls this function at least several
   * times per second. On most systems, getenv() walks a linear list that can
   * be quite large, and dlfunc() needs to walk a DAG of libraries.
   */
  static int screen = -1;
  static int do_delegate = 0;
  static int (*delegate)(SDL_Window*);
  static int delegate_initialised;

  if (screen < 0 && !do_delegate) {
    display_name = getenv("DISPLAY");

    if (parse_x11_screen(&screen, display_name) && screen >= 0)
      do_delegate = 0;
    else
      do_delegate = 1;
  }

  if (!do_delegate)
    return screen;

  /* Not on Zaphod mode, delegate to original */
  if (!delegate_initialised) {
    delegate =
      (int(*)(SDL_Window*))dlfunc(RTLD_NEXT, "SDL_GetWindowDisplayIndex");
    delegate_initialised = 1;

    if (!delegate) {
      warnx("Unable to delegate to SDL to choose display, assuming zero. %s",
#ifdef HAVE_DLERROR
            dlerror()
#else
            "(reason unknown because dlerror() unavailable)"
#endif
        );
    }
  }

  if (delegate)
    return (*delegate)(window);
  else
    return 0;
}
#endif

void select_window_bounds(SDL_Rect* window_bounds) {
  SDL_Rect display_bounds;
  int i, n, zaphod_screen;
  unsigned largest_index, largest_width;

  n = SDL_GetNumVideoDisplays();
  if (n <= 0) {
    warnx("Unable to determine number of video displays: %s",
          SDL_GetError());
    goto use_conservative_boundaries;
  }

  /* Generally prefer the display with the greatest width */
  largest_width = 0;
  largest_index = n;
  for (i = 0; i < n; ++i) {
    if (SDL_GetDisplayBounds(i, &display_bounds)) {
      warnx("Unable to determine bounds of display %d: %s",
            i, SDL_GetError());
    } else {
      /* A Zaphod configuration has all displays at (0,0) */
      might_be_zaphod &= (0 == display_bounds.x && 0 == display_bounds.y);

      if (display_bounds.w > (signed)largest_width) {
        largest_width = display_bounds.w;
        largest_index = i;
      }
    }
  }

  if (largest_index >= (unsigned)n) {
    warnx("Failed to query the bounds of any display...");
    goto use_conservative_boundaries;
  }

  /* If still a candidate for zaphod mode, and $DISPLAY indicates a particular
   * screen, force that screen. Otherwise, use the one we selected above.
   */
  if (might_be_zaphod && parse_x11_screen(&zaphod_screen, getenv("DISPLAY")) &&
      zaphod_screen >= 0 && zaphod_screen < n) {
    SDL_GetDisplayBounds(zaphod_screen, window_bounds);
  } else {
    SDL_GetDisplayBounds(largest_index, window_bounds);
  }
  return;

  use_conservative_boundaries:
  window_bounds->x = SDL_WINDOWPOS_UNDEFINED;
  window_bounds->y = SDL_WINDOWPOS_UNDEFINED;
  window_bounds->w = 640;
  window_bounds->h = 480;
}

int main(int argc, char** argv) {
  unsigned ww, wh;
  SDL_Window* screen;
  SDL_Renderer* renderer;
  SDL_Texture* rendertex;
  const int image_types = IMG_INIT_JPG | IMG_INIT_PNG;
  canvas* canv;
  crt_screen* crt;
  Uint32* framebuffer;
  game_state* state;
  SDL_Rect window_bounds;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    errx(EX_SOFTWARE, "Unable to initialise SDL: %s", SDL_GetError());

  atexit(SDL_Quit);

  select_window_bounds(&window_bounds);
  screen = SDL_CreateWindow("Praefectus",
                            window_bounds.x,
                            window_bounds.y,
                            window_bounds.w,
                            window_bounds.h,
                            0);
  if (!screen)
    errx(EX_OSERR, "Unable to create window: %s", SDL_GetError());

  if (image_types != (image_types & IMG_Init(image_types)))
    errx(EX_SOFTWARE, "Unable to init SDLIMG: %s", IMG_GetError());

  screen_pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
  if (!screen_pixel_format)
    errx(EX_UNAVAILABLE, "Unable to get ARGB8888 pixel format: %s",
         SDL_GetError());

  SDL_GetWindowSize(screen, (int*)&ww, (int*)&wh);

  renderer = SDL_CreateRenderer(screen, -1,
                                SDL_RENDERER_ACCELERATED |
                                SDL_RENDERER_PRESENTVSYNC);
  if (!renderer)
    renderer = SDL_CreateRenderer(screen, -1, 0);

  if (!renderer)
    errx(EX_SOFTWARE, "Unable to create SDL renderer: %s",
         SDL_GetError());

  rendertex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, ww, wh);
  if (!rendertex)
    errx(EX_SOFTWARE, "Unable to create SDL texture: %s",
         SDL_GetError());

  canv = canvas_new(240 * ww / wh, 240);
  crt = crt_screen_new(canv->w, canv->h, ww, wh, ww);
  framebuffer = xmalloc(sizeof(Uint32) * ww * wh);

  state = test_state_new();

  do {
    draw(canv, crt, state, screen);
    crt_screen_proj(framebuffer, crt);
    SDL_UpdateTexture(rendertex, NULL, framebuffer, ww * sizeof(Uint32));
    SDL_RenderCopy(renderer, rendertex, NULL, NULL);
    SDL_RenderPresent(renderer);

    if (handle_input(state)) break; /* quit */
    state = update(state);
  } while (state);

  free(canv);
  crt_screen_delete(crt);
  free(framebuffer);

  return 0;
}

static game_state* update(game_state* state) {
  static praef_instant prev;
  praef_instant now, elapsed;
  unsigned long long now_tmp;

  /* Busy-wait for the time to roll to the next instant. Sleep for 1ms between
   * attempts so we don't use the *whole* CPU doing nothing.
   */
  do {
    /* Get current time in instant */
    now_tmp = SDL_GetTicks();
    now_tmp *= SECOND;
    now = now_tmp / 1000;
    if (now == prev) SDL_Delay(1);
  } while (now == prev);

  elapsed = now - prev;
  prev = now;

  return (*state->update)(state, elapsed);
}

static void draw(canvas* canv, crt_screen* crt,
                 game_state* state,
                 SDL_Window* screen) {
  crt_colour palette[256];
  (*state->draw)(state, canv, palette);
  crt_screen_xfer(crt, canv, palette);
}

static int handle_input(game_state* state) {
  SDL_Event evt;

  while (SDL_PollEvent(&evt)) {
    switch (evt.type) {
    case SDL_QUIT: return 1;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      if (state->key)
        (*state->key)(state, &evt.key);
      break;

    case SDL_MOUSEMOTION:
      if (state->mmotion)
        (*state->mmotion)(state, &evt.motion);
      break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      if (state->mbutton)
        (*state->mbutton)(state, &evt.button);
      break;

    case SDL_MOUSEWHEEL:
      if (state->scroll)
        (*state->scroll)(state, &evt.wheel);
      break;

    case SDL_TEXTEDITING:
      if (state->txted)
        (*state->txted)(state, &evt.edit);
      break;

    case SDL_TEXTINPUT:
      if (state->txtin)
        (*state->txtin)(state, &evt.text);
      break;

    default: /* ignore */ break;
    }
  }

  return 0; /* continue running */
}
