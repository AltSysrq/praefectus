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
#ifndef GAME_STATE_H_
#define GAME_STATE_H_

#include <SDL.h>

#include "graphics/console.h"
#include "graphics/crt.h"

/**
 * The game_state is the high-level unit of activity control. There is exactly
 * one active game_state at a time, though the active state may delegate
 * control to another game_state. The active game state receives all update,
 * draw, and input events.
 *
 * The normal flow of events for a game_state is draw->input*->update->...
 *
 * Any input event function in the game_state structure may be NULL to indicate
 * it does not care about those events. update and draw are mandatory.
 */
typedef struct game_state_s game_state;

/**
 * Performs updates to this state, given the elapsed time, which will never be
 * zero. Returns the game_state that shall be active after this call. Returning
 * NULL terminates the program gracefully.
 *
 * Note that, in the case of changing states, it is this state's responsibility
 * to ensure that it destroys itself appropriately.
 */
typedef game_state* (*game_state_update_t)(game_state*, unsigned elapsed);
/**
 * Draws the graphical representation of this state onto the given canvas and
 * colour palette.
 */
typedef void (*game_state_draw_t)(game_state*, console*,
                                  crt_colour* palette);
/**
 * Called for every received keyboard event. This should not be used for
 * textual input processing.
 */
typedef void (*game_state_key_t)(game_state*, SDL_KeyboardEvent*);
/**
 * Called for every mouse button event.
 */
typedef void (*game_state_mbutton_t)(game_state*, SDL_MouseButtonEvent*);
/**
 * Called for every mouse motion event. It is up to the game_state to control
 * whether it wants absolute or relative mouse motion.
 */
typedef void (*game_state_mmotion_t)(game_state*, SDL_MouseMotionEvent*);
/**
 * Called for every scroll (mouse wheel) event.
 */
typedef void (*game_state_scroll_t)(game_state*, SDL_MouseWheelEvent*);
/**
 * Called for every text editing input event.
 *
 * @see game_state_txtin_t
 */
typedef void (*game_state_txted_t)(game_state*, SDL_TextEditingEvent*);
/**
 * Called for every text input event. For this (and txted) to be called,
 * SDL_StartTextInput() must have been called.
 *
 * The way these events work is described here:
 * http://wiki.libsdl.org/SDL_TextInputEvent
 */
typedef void (*game_state_txtin_t)(game_state*, SDL_TextInputEvent*);

struct game_state_s {
  game_state_update_t   update;
  game_state_draw_t     draw;
  game_state_key_t      key;
  game_state_mbutton_t  mbutton;
  game_state_mmotion_t  mmotion;
  game_state_scroll_t   scroll;
  game_state_txted_t    txted;
  game_state_txtin_t    txtin;
};

#endif /* GAME_STATE_H_ */
