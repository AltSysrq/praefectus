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
#ifndef GAME_OBJECT_H_
#define GAME_OBJECT_H_

#include "bsd.h"

#include <libpraefectus/system.h>
#include <libpraefectus/object.h>

#include "../graphics/canvas.h"

#define MAX_PROJECTILES 15

struct game_context_s;

/**
 * Object coordinates are stored in 11.5 fixed-point, and describe a
 * 2-dimensional toroidal space.
 */
typedef unsigned short game_object_coord;
typedef signed short game_object_scoord;
#define GOC_PIXEL_SIZE (1 << 5)

/**
 * Stores the state of a player-controlled object at some point in time. New
 * state objects are only produced when something occurs that prevents simple
 * linear extrapolation of the current state form the most recent core state.
 */
typedef struct {
  /**
   * The instant this state represents.
   */
  praef_instant instant;
  /**
   * The position of the object at the time of this state.
   */
  game_object_coord x, y;
  unsigned short
    /**
     * The X velocity (-1, 0, or +1) of the object.
     */
    xvel   : 2, /* offset -1 */
    /**
     * The Y velocity (-1, 0, or +1) of the object.
     */
    yvel   : 2, /* offset -1 */
    /**
     * The number of projectiles that are active at the time of this state.
     */
    nproj  : 4,
    /**
     * The number of hitpoints the object has, minus one.
     */
    hp     : 4;
  unsigned short score; /* offset +1 */
} game_object_core_state;

/**
 * Stores the state of a single projectile at some point in time. Whether a
 * projectile actually exists depends on whether its entry in the projectiles
 * state array exists and whether the current time is within the projectile
 * lifetime relative to the creation time of the projectile.
 */
typedef struct {
  praef_instant created_at, instant;
  game_object_coord x, y;
  signed char vx, vy;
} game_object_proj_state;

typedef struct game_object_s {
  praef_object self;

  struct game_context_s* context;

  /**
   * The screen name of the player controlling this object, encoded in CP437.
   */
  char screen_name[17];

  /**
   * The current time as perceived by the object.
   *
   * The object is not considered to exist if it has no state (see
   * core_num_states) or if this is later than the most recent state by more
   * than a certain time.
   */
  praef_instant now;
  /**
   * An array of all states in which this object has been. The current state is
   * located at core_num_states (if non-zero); the length of the array is
   * core_states_cap.
   */
  game_object_core_state* core_states;
  unsigned core_num_states, core_states_cap;
  /**
   * An array of all states of all projectiles ever associated with this
   * object. The current projectiles are in the range
   * [proj_num_states-current_state->nproj-1,proj_num_states-1]. The length of
   * the array is proj_states_cap.
   */
  game_object_proj_state* proj_states;
  unsigned proj_num_states, proj_states_cap;

  /**
   * The desired (sign-only) X and Y velocities of the player-controlled
   * object. These are used to produce new events to alter the actual
   * velocities.
   */
  signed want_xvel, want_yvel;

  SLIST_ENTRY(game_object_s) next;
} game_object;

game_object* game_object_new(struct game_context_s*,
                             praef_object_id);
void game_object_delete(game_object*);

/**
 * Sends any necessary passive events for the (local) object to the given
 * system. This includes initialisation, heartbeats, and control updates.
 */
void game_object_send_events(game_object*, praef_system*);
/**
 * Sends a single fire event for the given (local) object in the direction
 * implied by (xo,yo).
 */
void game_object_send_fire_one(const game_object*, praef_system*,
                               signed short xo, signed short yo);
/**
 * Equivalent to calling game_object_send_fire_one() MAX_PROJECTILES times with
 * some jitter added to xo and yo.
 */
void game_object_send_fire_all(const game_object*, praef_system*,
                               signed short xo, signed short yo);
/**
 * Renders the current state of the given object to the given canvas.
 */
void game_object_draw(canvas* dst, const game_object*,
                      game_object_coord cx, game_object_coord cy);

/**
 * Extracts the current state of the given game object into states representing
 * the current time. The projectiles array is normalised so that it only
 * contains live projectiles.
 *
 * @return Whether any state is meaningful; a value of 0 indicates that the
 * object does not exist or has not yet come into existence.
 */
int game_object_current_state(game_object_core_state*,
                              game_object_proj_state[MAX_PROJECTILES],
                              const game_object*);
/**
 * Updates the current state of the given object to match the given core and
 * projectile states. The projectile states are expected to be normalised as
 * per game_object_current_state().
 */
void game_object_put_state(game_object*,
                           const game_object_core_state*,
                           const game_object_proj_state[MAX_PROJECTILES]);

#endif /* GAME_OBJECT_H_ */
