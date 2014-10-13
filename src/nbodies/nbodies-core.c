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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <bsd.h>
#include <libpraefectus/system.h>
#include <libpraefectus/stdsys.h>

#include "nbodies-config.h"

/* Each object in the system has the state of a point and a velocity within
 * two-dimensional space. Objects do not directly track their current
 * position. Rather, they track the position and velocity as of the most recent
 * velocity change, and calculate the current position as necessary from that.
 *
 * There are two types of events: Position events and velocity events. A
 * position event is only valid when an object has no state; this event creates
 * a new state with a given position and no velocity. Velocity events are only
 * valid on objects that do have state; they push a new state with the new
 * velocity and position of the object.
 *
 * Objects that lack state are considered to be non-existent for purposes of
 * simulation and reporting.
 *
 * Position for both coordinates ranges from 0 to 2**26-1, and simply wraps
 * around on over- or underflow. Force calculations do not take wrap-around
 * into account. Conceptually, the coordinate space is 1024 pixels wide, with
 * each pixel having 65536 sub-coordinates for additinoal precision; the
 * simulation parameters are set with the intent of the simulation being
 * visualised this way.
 */

typedef unsigned coord;
typedef signed velocity;

#define COORD_MASK ((coord)(1024*65536-1))
#define EVENT_ENC_SIZE 9
/* The acceleration provided by an object at distance 65536 ("one pixel"), in
 * coordinates per instant per event.
 *
 * Note that distances are always rounded down to a "pixel" boundary, and that
 * a distance of 0 has no effect.
 */
#define FORCE (65536/4)

unsigned global_clock = 0;

typedef struct body_state_at_instant_s {
  coord x, y;
  velocity vx, vy;
  praef_instant instant;

  SLIST_ENTRY(body_state_at_instant_s) previous;
} body_state_at_instant;

typedef struct body_state_s {
  praef_object self;
  praef_instant now;

  SLIST_HEAD(,body_state_at_instant_s) state;
  SLIST_ENTRY(body_state_s) next;
} body_state;

typedef struct {
  praef_event self;

  velocity vx, vy;
} body_velocity_event;

typedef struct {
  praef_event self;

  coord x, y;
} body_position_event;

typedef struct nbodies_instance_s {
  praef_app* app;
  praef_std_state state;
  praef_system* sys;
  SLIST_HEAD(,body_state_s) bodies;

  int is_alive;
  unsigned event_optimism;
  unsigned ticks_without_update;

  SLIST_ENTRY(nbodies_instance_s) next;
} nbodies_instance;

static SLIST_HEAD(,nbodies_instance_s) instances =
  SLIST_HEAD_INITIALIZER(instances);


static inline void* xmalloc(size_t sz) {
  void* ret = malloc(sz);
  if (!ret)
    err(EX_UNAVAILABLE, "Out of memory");

  return ret;
}

#define AVER(condition) do {                                    \
    if (!(condition))                                           \
      errx(EX_SOFTWARE, "Condition " #condition " failed");     \
  } while (0)

static praef_event* decode_event(praef_app*, praef_instant, praef_object_id,
                                 praef_event_serial_number,
                                 const void* data, size_t sz);
static void create_position_event(unsigned char[EVENT_ENC_SIZE],
                                  coord, coord);
static void create_velocity_event(unsigned char[EVENT_ENC_SIZE],
                                  velocity, velocity);
static void create_node_object(praef_app*, praef_object_id);
static void body_step(body_state*, nbodies_instance*);
static void body_rewind(body_state*, praef_instant);
static int body_current_state(body_state_at_instant*, const body_state*);
static void body_position_event_apply(
  body_state*, const body_position_event*, nbodies_instance*);
static void body_velocity_event_apply(
  body_state*, const body_velocity_event*, nbodies_instance*);
static unsigned optimistic_event_test(praef_app*, const praef_event*);

static void log_prefix(praef_app* app) {
  nbodies_instance* nbodies = praef_stdsys_userdata(app);
  praef_object_id id = 0;

  if (nbodies->sys)
    id = praef_system_get_local_id(nbodies->sys);

  if (id)
    fprintf(stderr, "%d: %08X: ", global_clock, id);
  else
    fprintf(stderr, "%d: [%p]: ", global_clock, app);
}

static void app_acquire_id(praef_app* app, praef_object_id id) {
  log_prefix(app);
  fprintf(stderr, "Acquired id %08X\n", id);
}
static void app_discover_node(praef_app* app,
                              const PraefNetworkIdentifierPair_t* net,
                              praef_object_id id) {
  log_prefix(app);
  fprintf(stderr, "Discovered node %08X\n", id);
}
static void app_join_tree_traversed(praef_app* app) {
  log_prefix(app);
  fprintf(stderr, "Join tree traversed\n");
}
static void app_ht_scan_progress(praef_app* app, unsigned n, unsigned d) {
  log_prefix(app);
  fprintf(stderr, "HT scan progress: %d/%d\n", n, d);
}
static void app_awaiting_stability(praef_app* app, praef_object_id node,
                                   praef_instant systime,
                                   praef_instant committed,
                                   praef_instant validated) {
  log_prefix(app);
  fprintf(stderr, "Awaiting stability on %08X: s=%d, c=%d, v=%d\n",
          node, systime, committed, validated);
}
static void app_information_complete(praef_app* app) {
  log_prefix(app);
  fprintf(stderr, "Information complete\n");
}
static void app_clock_synced(praef_app* app) {
  log_prefix(app);
  fprintf(stderr, "Clock synced\n");
}
static void app_gained_grant(praef_app* app) {
  log_prefix(app);
  fprintf(stderr, "Gained grant\n");
}
static void app_log(praef_app* app, const char* str) {
  log_prefix(app);
  fprintf(stderr, "%s\n", str);
}

static void cycle(void);
static void spawn_next(void);
static void cycle_one(nbodies_instance*);
static void update_velocity(nbodies_instance*);
static void nbodies_instance_write_state(nbodies_instance*);
static unsigned isqrt(unsigned);

#ifdef __GNUC__
/* Stop GCC complaining about our stricter types on main() */
#pragma GCC diagnostic ignored "-Wmain"
#endif
int main(unsigned argc, const char*const* argv) {
  unsigned i;

  nbodies_config_init(argv, argc);

  for (i = 0; i < nbodies_config_num_steps(); ++i)
    cycle();

  /* TODO: Free memory for checking by valgrind */

  return 0;
}

static praef_event* decode_event(praef_app* app, praef_instant instant,
                                 praef_object_id object,
                                 praef_event_serial_number serno,
                                 const void* vdata, size_t sz) {
  const unsigned char* data = vdata;
  body_velocity_event* vevt;
  body_position_event* bevt;

  if (sz != EVENT_ENC_SIZE) return 0;

  switch (data[0]) {
  case 0:
    bevt = xmalloc(sizeof(body_position_event));
    bevt->self.instant = instant;
    bevt->self.object = object;
    bevt->self.serial_number = serno;
    bevt->self.apply = (praef_event_apply_t)body_position_event_apply;
    bevt->self.free = free;
    bevt->x = le32dec(data+1);
    bevt->y = le32dec(data+5);
    return (praef_event*)bevt;

  case 1:
    vevt = xmalloc(sizeof(body_velocity_event));
    vevt->self.instant = instant;
    vevt->self.object = object;
    vevt->self.serial_number = serno;
    vevt->self.apply = (praef_event_apply_t)body_velocity_event_apply;
    vevt->self.free = free;
    vevt->vx = le32dec(data+1);
    vevt->vy = le32dec(data+5);
    return (praef_event*)vevt;

  default:
    return NULL;
  }
}

static void create_position_event(unsigned char dst[EVENT_ENC_SIZE],
                                  coord x, coord y) {
  dst[0] = 0;
  le32enc(dst+1, x);
  le32enc(dst+5, y);
}

static void create_velocity_event(unsigned char dst[EVENT_ENC_SIZE],
                                  velocity vx, velocity vy) {
  dst[0] = 1;
  le32enc(dst+1, vx);
  le32enc(dst+5, vy);
}

static void create_node_object(praef_app* app, praef_object_id id) {
  nbodies_instance* nbodies = praef_stdsys_userdata(app);
  body_state* obj = xmalloc(sizeof(body_state));

  obj->self.id = id;
  obj->self.step = (praef_object_step_t)body_step;
  obj->self.rewind = (praef_object_rewind_t)body_rewind;
  SLIST_INIT(&obj->state);
  obj->now = 0;
  SLIST_INSERT_HEAD(&nbodies->bodies, obj, next);

  AVER(NULL ==
       praef_context_add_object(nbodies->state.context, (praef_object*)obj));
}

static void body_step(body_state* this, nbodies_instance* nbodies) {
  ++this->now;
}

static void body_rewind(body_state* this, praef_instant then) {
  body_state_at_instant* s;

  this->now = then;

  while (!SLIST_EMPTY(&this->state) &&
         SLIST_FIRST(&this->state)->instant >= then) {
    s = SLIST_FIRST(&this->state);
    SLIST_REMOVE_HEAD(&this->state, previous);
    free(s);
  }
}

static int body_current_state(body_state_at_instant* dst,
                              const body_state* obj) {
  body_state_at_instant* s;
  unsigned delta;

  if (SLIST_EMPTY(&obj->state)) return 0;

  s = SLIST_FIRST(&obj->state);
  delta = obj->now - s->instant;
  dst->x = (s->x + delta*s->vx) & COORD_MASK;
  dst->y = (s->y + delta*s->vy) & COORD_MASK;
  dst->vx = s->vx;
  dst->vy = s->vy;
  return 1;
}

static void body_position_event_apply(body_state* obj,
                                      const body_position_event* evt,
                                      nbodies_instance* nbodies) {
  body_state_at_instant* state;

  if (!SLIST_EMPTY(&obj->state)) return;

  state = xmalloc(sizeof(body_state_at_instant));
  state->x = evt->x & COORD_MASK;
  state->y = evt->y & COORD_MASK;
  state->vx = 0;
  state->vy = 0;
  state->instant = evt->self.instant;
  SLIST_INSERT_HEAD(&obj->state, state, previous);
}

static void body_velocity_event_apply(body_state* obj,
                                      const body_velocity_event* evt,
                                      nbodies_instance* nbodies) {
  body_state_at_instant* new;

  if (SLIST_EMPTY(&obj->state)) return;

  new = xmalloc(sizeof(body_state_at_instant));
  AVER(body_current_state(new, obj));
  new->vx = evt->vx;
  new->vy = evt->vy;
  new->instant = evt->self.instant;
  SLIST_INSERT_HEAD(&obj->state, new, previous);
}

static unsigned optimistic_event_test(praef_app* app, const praef_event* event) {
  nbodies_instance* this = praef_stdsys_userdata(app);

  return this->event_optimism;
}

static void cycle(void) {
  nbodies_instance* instance;
  unsigned i, n;

  n = nbodies_config_step();
  for (i = 0; i < n; ++i)
    spawn_next();

  SLIST_FOREACH(instance, &instances, next)
    cycle_one(instance);

  ++global_clock;
}

static void spawn_next(void) {
  nbodies_instance* this = xmalloc(sizeof(nbodies_instance));
  memset(this, 0, sizeof(nbodies_instance));

  AVER(praef_std_state_init(&this->state));
  AVER(this->app = praef_stdsys_new(&this->state));
  praef_stdsys_set_userdata(this->app, this);
  praef_stdsys_optimistic_events(this->app, optimistic_event_test);
  this->app->create_node_object = create_node_object;
  this->app->decode_event = decode_event;
  this->app->acquire_id_opt = app_acquire_id;
  this->app->discover_node_opt = app_discover_node;
  this->app->join_tree_traversed_opt = app_join_tree_traversed;
  this->app->ht_scan_progress_opt = app_ht_scan_progress;
  this->app->awaiting_stability_opt = app_awaiting_stability;
  this->app->information_complete_opt = app_information_complete;
  this->app->clock_synced_opt = app_clock_synced;
  this->app->gained_grant_opt = app_gained_grant;
  this->app->log_opt = app_log;
  this->sys = nbodies_config_create_system(this->app);
  praef_stdsys_set_system(this->app, this->sys);
  this->is_alive = 0;
  this->event_optimism = nbodies_config_optimism();
  this->ticks_without_update = 0;

  SLIST_INSERT_HEAD(&instances, this, next);
}

static void cycle_one(nbodies_instance* nbodies) {
  unsigned char evt[EVENT_ENC_SIZE];
  praef_system_status status ;

  status = praef_system_advance(nbodies->sys, 1);
  switch (status) {
  case praef_ss_anonymous:
  case praef_ss_pending_grant:
    /* Normal part of connection, don't care */
    break;

  case praef_ss_ok:
    /* If not yet alive, set the object's position. */
    if (!nbodies->is_alive) {
      nbodies->is_alive = 1;

      create_position_event(
        evt, rand() & COORD_MASK, rand() & COORD_MASK);
      AVER(praef_system_add_event(nbodies->sys, evt, EVENT_ENC_SIZE));
    }

    /* Update velocity if needed */
    if (++nbodies->ticks_without_update >= EVENT_INTERVAL) {
      update_velocity(nbodies);
      nbodies->ticks_without_update = 0;
    }
    break;

  default: errx(EX_SOFTWARE, "System gained unexpected status %d", status);
  }

  nbodies_instance_write_state(nbodies);
}

static void update_velocity(nbodies_instance* nbodies) {
  unsigned char evt[EVENT_ENC_SIZE];
  signed ax = 0, ay = 0;
  signed dx, dy;
  unsigned dist;
  body_state* self, * obj;
  praef_object_id self_id;
  body_state_at_instant self_state, obj_state;

  self_id = praef_system_get_local_id(nbodies->sys);
  if (!self_id) return;

  self = (body_state*)praef_context_get_object(
    nbodies->state.context, self_id);
  AVER(self);

  if (!body_current_state(&self_state, self)) return;

  SLIST_FOREACH(obj, &nbodies->bodies, next) {
    if (obj == self) continue;
    if (!body_current_state(&obj_state, obj)) continue;

    dx = obj_state.x - self_state.x;
    dy = obj_state.y - self_state.y;
    dx /= 65536;
    dy /= 65536;
    dist = isqrt(dx*dx + dy*dy);

    if (dist) {
      ax += FORCE * dx / (signed)dist;
      ay += FORCE * dy / (signed)dist;
    }
  }

  create_velocity_event(evt, self_state.vx*31/32 + ax, self_state.vy*31/32 + ay);
  AVER(praef_system_add_event(nbodies->sys, evt, EVENT_ENC_SIZE));
}

static void nbodies_instance_write_state(nbodies_instance* nbodies) {
  praef_object_id self_id = praef_system_get_local_id(nbodies->sys);
  praef_instant now = praef_system_get_clock(nbodies->sys)->monotime;
  body_state* obj;
  body_state_at_instant state;

  if (!self_id) return;

  SLIST_FOREACH(obj, &nbodies->bodies, next) {
    if (!body_current_state(&state, obj)) continue;

    printf("%d,%08X,%08X,%07X,%07X,%+d,%+d,%d\n",
           now, self_id, obj->self.id,
           state.x, state.y, state.vx, state.vy,
           praef_system_get_latency_to(nbodies->sys, obj->self.id));
  }
}

unsigned isqrt(unsigned n) {
  unsigned x0, x1 = n, delta;
  if (!n) return 0;

  do {
    x0 = x1;
    x1 = (x0 + n/x0) / 2;
    delta = x0 - x1;
  } while (delta && ~delta && delta-1);

  return x1;
}
