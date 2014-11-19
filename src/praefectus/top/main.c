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
#include "../common.h"
#include "../game-state.h"
#include "../global-config.h"
#include "../alloc.h"
#include "../graphics/font.h"
#include "../graphics/canvas.h"
#include "main-menu.h"
#include "../game/gameplay.h"

/* Move this somewhere else if anything else winds up needing it. */
SDL_PixelFormat* screen_pixel_format;
#define NOMINAL_HEIGHT 480

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

/* Since surface blits in SDL2 aren't async (as they could be in SDL1.2), we
 * instead perform the CRT rendering for the next frame while the current one
 * blits on a separate thread.
 *
 * The background thread waits on the crt_ready semaphore. Once acquired
 * (decrementing it back to zero), it renders into the framebuffer_back global,
 * then posts the framebuffer_ready semaphore.
 *
 * The main thread waits on the framebuffer_ready semaphore, then swaps the
 * framebuffers and posts the crt_ready semaphore, then blits the front
 * framebuffer (which isn't global).
 *
 * Initially, the framebuffer_ready has a value of 1, and the crt_ready of
 * zero.
 */
static crt_screen* crt;
static Uint32* framebuffer_back;
static SDL_sem* crt_ready, * framebuffer_ready;
static int render_crt_background(void*);

static unsigned round_to_multiple(unsigned n, unsigned m) {
  return (n + m-1) / m * m;
}

int main(int argc, char** argv) {
  unsigned ww, wh;
  SDL_Window* screen;
  SDL_Renderer* renderer;
  SDL_Texture* rendertex;
  canvas* canv;
  Uint32* framebuffer_front, * framebuffer_tmp, * framebuffer_both;
  game_state* state;
  SDL_Rect window_bounds;
  SDL_Thread* crt_render_thread;
  unsigned num_frames_since_fps_report = 0, last_fps_report = SDL_GetTicks();

  load_config();

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

  screen_pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
  if (!screen_pixel_format)
    errx(EX_UNAVAILABLE, "Unable to get ARGB8888 pixel format: %s",
         SDL_GetError());

  SDL_GetWindowSize(screen, (int*)&ww, (int*)&wh);

  /* For lack of a better place, the reasons behind the SDL rendering used in
   * Praefectus follows.
   *
   * The original code simply used an accelerated renderer if possible, and
   * then fell back on "whatever". However, at high resolutions, it pegged the
   * CPU completely, so I investigated software blitting, including with the
   * older SDL 1.2 version. Results on my desktop follow. CPU percentages are
   * Praefectus+Xorg, though Xorg is omitted for hardware blitting. "Overhead"
   * refers to the time between completing the rendering pass and the instant
   * before the call to SDL_RenderPresent()/SDL_Flip().
   *
   * Blitting           1280x1024                       1920x1080
   * SDL1.2 Soft        50%+47%; 1ms overhead           70%+40%; 2ms overhead
   * SDL2.0 Hard        80%; 6ms overhead               pegged; 10ms overhead
   * SDL2.0 Soft        50%+45%; 4ms overhead           60%+40%; 7ms overhead
   *
   * SDL2.0 software somehow interacts more poorly with the FreeBSD scheduler
   * than SDL1.2. The former runs smoothely at 1280x1024 and is only mildly
   * jerky at 1920x1080. The latter is jerky at both. The fact that SDL2.0 only
   * supports synchronous blits makes things even worse, as X and the
   * application end up alternately blocking on each other, while 110% of the
   * CPU is really required to run at the full frame-rate. Thus SDL2.0 Software
   * should be avoided.
   *
   * Though SDL2.0 hardware blitting incurs greater raw overhead than SDL1.2
   * software, it actually runs smoother (at least with NVidia GLX) since it
   * avoids lots of context switches. The biggest advantage of SDL1.2 software
   * is its support for async blitting. However, we can get most of this
   * benefit by rendering the next frame on a separate thread while the
   * previous one is blitted. In this case, the overhead is effectively
   * eliminated since it is below the time spent rendering a frame. The only
   * real cost is some extra latency in getting things onto the screen.
   */
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

  crt_ready = SDL_CreateSemaphore(0);
  framebuffer_ready = SDL_CreateSemaphore(1);
  if (!crt_ready || !framebuffer_ready)
    errx(EX_UNAVAILABLE, "Failed to create semaphores: %s",
         SDL_GetError());

  crt_render_thread = SDL_CreateThread(render_crt_background,
                                       "render-crt-background",
                                       NULL);
  if (!crt_render_thread)
    errx(EX_UNAVAILABLE, "Failed to spawn render thread: %s",
         SDL_GetError());

  canv = canvas_new(round_to_multiple(NOMINAL_HEIGHT * ww / wh, FONT_CHARW),
                    round_to_multiple(NOMINAL_HEIGHT, FONT_CHARH));
  /* TODO: Configurable BEL behaviour */
  crt = crt_screen_new(canv->w, canv->h, ww, wh, ww);
  framebuffer_both = xmalloc(2 * sizeof(Uint32) * ww * wh);
  framebuffer_back = framebuffer_both;
  framebuffer_front = framebuffer_both + ww*wh;
  memset(framebuffer_back, 0, 2 * sizeof(Uint32) * ww * wh);

  /* state = main_menu_new(canv);*/
  state = gameplay_state_test();

  do {
    draw(canv, crt, state, screen);
    SDL_SemWait(framebuffer_ready);
    framebuffer_tmp = framebuffer_front;
    framebuffer_front = framebuffer_back;
    framebuffer_back = framebuffer_tmp;
    SDL_SemPost(crt_ready);
    SDL_UpdateTexture(rendertex, NULL, framebuffer_front, ww * sizeof(Uint32));
    SDL_RenderCopy(renderer, rendertex, NULL, NULL);
    SDL_RenderPresent(renderer);

    if (handle_input(state)) break; /* quit */
    state = update(state);

    ++num_frames_since_fps_report;
    if (SDL_GetTicks() - last_fps_report > 3000) {
      last_fps_report = SDL_GetTicks();
      printf("FPS: %3d\n", num_frames_since_fps_report / 3);
      num_frames_since_fps_report = 0;
    }
  } while (state);

  /* Before freeing stuff, wait for the background thread to have finished its
   * final pass.
   *
   * (Don't bother trying to stop it; after this call, it'll be eternally
   * frozen on that semaphore while we finish cleanup up, and then it'll die
   * along with the whole process.)
   */
  SDL_SemWait(framebuffer_ready);

  free(canv);
  crt_screen_delete(crt);
  free(framebuffer_both);

  save_config();

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

static int render_crt_background(void* _) {
  for (;;) {
    SDL_SemWait(crt_ready);
    crt_screen_proj(framebuffer_back, crt);
    SDL_SemPost(framebuffer_ready);
  }

  /* unreachable */
  abort();
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
