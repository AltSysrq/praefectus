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

#include "test.h"

#include <stdlib.h>

#include <libpraefectus/system.h>
#include <libpraefectus/stdsys.h>
#include <libpraefectus/virtual-bus.h>
#include <libpraefectus/-system.h>

/**
 * @file
 *
 * These tests create full networks of actual praef_systems with actual
 * underlying application contexts. Because of this, certain aspects of the
 * tests can on very rare occasions randomly fail; in particular, it is
 * possible for a test to happen to create a chimera node, which will result in
 * the colliding nodes getting kicked.
 *
 * An object is modelled as a single integer, and can store 2048 states
 * maximum. Each event has a single byte of payload; applying the event adds
 * that byte to a cumulative hash in the state of the object. There is no
 * provision to check for correct timing of events, as that is throughly
 * handled by the context tests.
 *
 * These tests only touch non-public APIs in order to perturb nodes into
 * particular invalid states.
 *
 * At any given moment, a system can be in one of four states. Inactive
 * systems are not advanced at all. Idle systems are advanced, but produce no
 * application events. Active systems advance and produce one event per
 * frame. Semiactive systems advance and produce one event per 10 frames.
 */

defsuite(libpraefectus_system_fullstack);

#define NUM_NODES 8
#define NUM_INST 4096

static enum {
  ts_inactive = 0,
  ts_idle,
  ts_semiactive,
  ts_active
} activity[NUM_NODES];
static praef_system_status status[NUM_NODES];
static praef_system* sys[NUM_NODES];
static praef_std_state state[NUM_NODES];
static praef_app* app[NUM_NODES];
static praef_virtual_network* vnet;
static praef_virtual_bus* vbus[NUM_NODES];
static praef_message_bus* bus[NUM_NODES];
static praef_virtual_network_link* link_from_to[NUM_NODES][NUM_NODES];
static const PraefNetworkIdentifierPair_t* net_id[NUM_NODES];

typedef struct {
  praef_object self;
  praef_instant now;
  unsigned state[NUM_INST];
} test_object;

typedef struct {
  praef_event self;
  unsigned char val;
} test_event;

static test_object objects[NUM_NODES][NUM_NODES];
static unsigned num_objects[NUM_NODES];

static test_object* object_at(unsigned perceived_by,
                              unsigned subject) {
  unsigned i;

  if (!sys[subject]->local_node) return NULL;

  for (i = 0; i < num_objects[perceived_by]; ++i)
    if (objects[perceived_by][i].self.id == sys[subject]->local_node->id)
      return &objects[perceived_by][i];

  return NULL;
}

static unsigned app_ix(void* a) {
  return ((praef_app**)praef_stdsys_userdata(a)) - app;
}

static void test_object_step(test_object* this, praef_userdata userdata) {
  ++this->now;
  this->state[this->now] = this->state[this->now-1];
}

static void test_object_rewind(test_object* this, praef_instant then) {
  this->now = then;
  if (then)
    this->state[then] = this->state[then-1];
  else
    this->state[then] = 0;
}

static void test_event_apply(test_object* target, test_event* this,
                             praef_userdata _) {
  target->state[target->now] *= 31;
  target->state[target->now] += this->val;
}

static void create_node_object(praef_app* this, praef_object_id id) {
  unsigned ix = app_ix(this);
  test_object* object = &objects[ix][num_objects[ix]++];
  object->self.id = id;
  object->self.step = (praef_object_step_t)test_object_step;
  object->self.rewind = (praef_object_rewind_t)test_object_rewind;

  ck_assert_ptr_eq(NULL,
                   praef_context_add_object(
                     state[ix].context, (praef_object*)object));
}

static praef_event* decode_event(praef_app* this, praef_instant instant,
                                 praef_object_id object,
                                 praef_event_serial_number sn,
                                 const void* data, size_t sz) {
  test_event* evt;

  if (1 != sz) return NULL;

  evt = malloc(sizeof(test_event));
  evt->self.instant = instant;
  evt->self.object = object;
  evt->self.serial_number = sn;
  evt->self.apply = (praef_event_apply_t)test_event_apply;
  evt->self.free = free;
  evt->val = *(const unsigned char*)data;
  return (praef_event*)evt;
}

static void applog(praef_app* _, const char* str) {
  puts(str);
}

defsetup {
  /* This is only to complement the teardown block. Actual setup is done by
   * set_up(), so the tests can specify different parameters.
   */
}

defteardown {
  unsigned i;

  for (i = 0; i < NUM_NODES; ++i) {
    praef_system_delete(sys[i]);
    praef_stdsys_delete(app[i]);
    praef_std_state_cleanup(&state[i]);
  }

  praef_virtual_network_delete(vnet);

  /* Zero everything for checking by valgrind and to ensure the next test (if
   * running CK_FORK=no) starts clean.
   */
  memset(sys, 0, sizeof(sys));
  memset(app, 0, sizeof(app));
  memset(state, 0, sizeof(state));
  vnet = NULL;
  memset(vbus, 0, sizeof(vbus));
  memset(bus, 0, sizeof(bus));
  memset(link_from_to, 0, sizeof(link_from_to));
  memset(net_id, 0, sizeof(net_id));
  memset(objects, 0, sizeof(objects));
  memset(num_objects, 0, sizeof(num_objects));
  memset(activity, 0, sizeof(activity));
  memset(status, 0, sizeof(status));
}

static void set_up(unsigned std_latency,
                   unsigned min_latency, unsigned max_latency,
                   praef_system_profile profile) {
  unsigned i, j;

  printf("\n---\n");

  vnet = praef_virtual_network_new();
  for (i = 0; i < NUM_NODES; ++i) {
    praef_std_state_init(&state[i]);
    vbus[i] = praef_virtual_network_create_node(vnet);
    bus[i] = praef_virtual_bus_mb(vbus[i]);
    net_id[i] = praef_virtual_bus_address(vbus[i]);
    app[i] = praef_stdsys_new(&state[i]);
    sys[i] = praef_system_new(app[i], bus[i], net_id[i],
                              std_latency, profile,
                              praef_siv_any,
                              praef_snl_any,
                              8+PRAEF_HLMSG_MTU_MIN);
    praef_stdsys_set_system(app[i], sys[i]);
    praef_stdsys_set_userdata(app[i], &app[i]);

    app[i]->decode_event = decode_event;
    app[i]->create_node_object = create_node_object;
    app[i]->log_opt = applog;
  }

  for (i = 0; i < NUM_NODES; ++i) {
    for (j = 0; j < NUM_NODES; ++j) {
      if (i == j) continue;
      link_from_to[i][j] = praef_virtual_bus_link(vbus[i], vbus[j]);
      link_from_to[i][j]->base_latency = min_latency;
      link_from_to[i][j]->variable_latency = max_latency - min_latency;
      link_from_to[i][j]->firewall_grace_period = std_latency;
    }
  }
}

static void advance_one(void) {
  unsigned i;
  unsigned char byte;

  for (i = 0; i < NUM_NODES; ++i) {
    if (ts_inactive != activity[i])
      status[i] = praef_system_advance(sys[i], 1);

    if (ts_active == activity[i] ||
        (ts_semiactive == activity[i] &&
         0 == sys[i]->clock.ticks % 10)) {
      byte = rand();
      ck_assert(praef_system_add_event(sys[i], &byte, 1));
    }
  }

  praef_virtual_network_advance(vnet, 1);
}

static void advance(unsigned n) {
  while (n--)
    advance_one();
}

deftest(can_run_alone) {
  set_up(5, 0, 0, praef_sp_lax);

  praef_system_bootstrap(sys[0]);
  activity[0] = ts_active;
  advance(1024);
  activity[0] = ts_idle;
  advance(1);

  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert_int_eq(1, num_objects[0]);
  ck_assert_int_ne(0, objects[0][0].state[1000]);
}

deftest(can_join_system_with_populated_events) {
  set_up(5, 0, 0, praef_sp_lax);

  praef_system_bootstrap(sys[0]);
  activity[0] = ts_active;
  advance(100);
  praef_system_connect(sys[1], net_id[0]);
  activity[1] = ts_idle;
  advance(512);
  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert_int_eq(praef_ss_ok, status[1]);
  activity[1] = ts_active;
  advance(100);
  activity[0] = activity[1] = ts_idle;
  advance(100);

  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert_int_eq(praef_ss_ok, status[1]);
  ck_assert_int_eq(object_at(0,0)->state[750], object_at(1,0)->state[750]);
  ck_assert_int_eq(object_at(0,1)->state[750], object_at(1,1)->state[750]);
}

deftest(can_join_system_via_non_bootstrap_node) {
  set_up(5, 0, 0, praef_sp_lax);

  praef_system_bootstrap(sys[0]);
  activity[0] = ts_active;
  praef_system_connect(sys[1], net_id[0]);
  activity[1] = ts_idle;
  advance(512);
  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert_int_eq(praef_ss_ok, status[1]);
  activity[1] = ts_active;
  praef_system_connect(sys[2], net_id[1]);
  activity[2] = ts_idle;
  advance(512);
  activity[0] = activity[1] = ts_idle;
  advance(100);

  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert_int_eq(praef_ss_ok, status[1]);
  ck_assert_int_eq(praef_ss_ok, status[2]);
  ck_assert_int_eq(object_at(0,0)->state[600], object_at(2,0)->state[600]);
  ck_assert_int_eq(object_at(0,2)->state[600], object_at(2,2)->state[600]);
}

deftest(two_node_partition_results_in_netsplit) {
  /* This simultaneously tests the ability to handle the disappearance of the
   * bootstrap and non-bootstrap nodes, and that the system continues
   * functioning in the case where a two-node system loses one of the nodes.
   */

  set_up(5, 0, 0, praef_sp_lax);

  praef_system_bootstrap(sys[0]);
  activity[0] = ts_idle;
  praef_system_connect(sys[1], net_id[0]);
  activity[1] = ts_idle;
  advance(512);
  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert_int_eq(praef_ss_ok, status[1]);

  printf("Expect nodes %08X, %08X to become negative\n",
         1, sys[1]->local_node->id);
  link_from_to[0][1]->reliability = 0;
  link_from_to[1][0]->reliability = 0;
  advance(512);
  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert(!praef_node_is_alive(praef_system_get_node(
                                   sys[0], sys[1]->local_node->id)));
  ck_assert(!praef_node_is_alive(praef_system_get_node(sys[1], 1)));
}

deftest(vanising_node_gets_kicked) {
  set_up(5, 0, 0, praef_sp_lax);

  praef_system_bootstrap(sys[0]);
  activity[0] = ts_idle;
  praef_system_connect(sys[1], net_id[0]);
  activity[1] = ts_idle;
  praef_system_connect(sys[2], net_id[0]);
  activity[2] = ts_idle;
  advance(512);
  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert_int_eq(praef_ss_ok, status[1]);
  ck_assert_int_eq(praef_ss_ok, status[2]);

  printf("Expect node %08X to become negative\n", 1);
  activity[0] = ts_inactive;
  advance(512);
  ck_assert_int_eq(praef_ss_ok, status[1]);
  ck_assert_int_eq(praef_ss_ok, status[2]);
  ck_assert(!praef_node_is_alive(praef_system_get_node(sys[1], 1)));
  ck_assert(!praef_node_is_alive(praef_system_get_node(sys[2], 1)));
}

deftest(network_partition_detected_5_nodes) {
  unsigned i, j, is, js;

  set_up(5, 0, 0, praef_sp_lax);

  praef_system_bootstrap(sys[0]);
  activity[0] = ts_idle;
  for (i = 1; i < 5; ++i) {
    praef_system_connect(sys[i], net_id[0]);
    activity[i] = ts_idle;
  }
  advance(512);
  for (i = 0; i < 5; ++i)
    ck_assert_int_eq(praef_ss_ok, status[i]);

  /* Partition into (0,1), (2,3,4) */
  for (i = 0; i < 5; ++i) {
    is = (i < 2);
    for (j = 0; j < 5; ++j) {
      js = (j < 2);
      if (is != js)
        link_from_to[i][j]->reliability = 0;
    }
  }

  printf("Expect all nodes to become negative to someone\n");
  advance(512);
  ck_assert_int_eq(praef_ss_partitioned, status[0]);
  ck_assert_int_eq(praef_ss_partitioned, status[1]);
  for (i = 2; i < 5; ++i) {
    ck_assert_int_eq(praef_ss_ok, status[i]);
    for (j = 0; j < 2; ++j) {
      ck_assert(!praef_node_is_alive(
                  praef_system_get_node(
                    sys[i], sys[j]->local_node->id)));
    }
  }
}

deftest(can_kick_nodes) {
  unsigned i;

  set_up(5, 0, 0, praef_sp_lax);

  praef_system_bootstrap(sys[0]);
  activity[0] = ts_idle;
  for (i = 1; i < 4; ++i) {
    praef_system_connect(sys[i], net_id[0]);
    activity[i] = ts_idle;
  }
  advance(512);
  for (i = 0; i < 4; ++i)
    ck_assert_int_eq(praef_ss_ok, status[i]);

  /* Nodes 2 and 3 spontaneously decide node 1 needs to go */
  printf("Forcing %08X to become negative\n",
         sys[1]->local_node->id);
  praef_node_negative(praef_system_get_node(sys[2], sys[1]->local_node->id),
                      "Forced by test");
  praef_node_negative(praef_system_get_node(sys[3], sys[1]->local_node->id),
                      "Forced by test");

  advance(256);
  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert(!praef_node_is_alive(praef_system_get_node(
                                   sys[0], sys[1]->local_node->id)));
  ck_assert_int_eq(praef_ss_kicked, status[1]);
  ck_assert_int_eq(praef_ss_ok, status[2]);
  ck_assert(!praef_node_is_alive(praef_system_get_node(
                                   sys[2], sys[1]->local_node->id)));
  ck_assert_int_eq(praef_ss_ok, status[3]);
  ck_assert(!praef_node_is_alive(praef_system_get_node(
                                   sys[3], sys[1]->local_node->id)));
}

deftest(can_disconnect_gracefully) {
  unsigned i;

  set_up(5, 0, 0, praef_sp_lax);

  /* Use 3 nodes so that one vote is not enough */
  praef_system_bootstrap(sys[0]);
  activity[0] = ts_idle;
  praef_system_connect(sys[1], net_id[0]);
  activity[1] = ts_idle;
  praef_system_connect(sys[2], net_id[0]);
  activity[2] = ts_idle;

  advance(512);
  for (i = 0; i < 3; ++i)
    ck_assert_int_eq(praef_ss_ok, status[i]);

  printf("Expect %08X to become negative\n", 1);
  praef_system_disconnect(sys[0]);
  advance(256);

  ck_assert_int_eq(praef_ss_kicked, status[0]);
  ck_assert_int_eq(praef_ss_ok, status[1]);
  ck_assert(!praef_node_is_alive(praef_system_get_node(sys[1], 1)));
  ck_assert_int_eq(praef_ss_ok, status[2]);
  ck_assert(!praef_node_is_alive(praef_system_get_node(sys[2], 1)));
}

deftest(can_connect_on_lossy_network) {
  set_up(10, 5, 15, praef_sp_lax);
  /* 12% packet loss, 6.25% packet duplication */
  link_from_to[0][1]->reliability = 57344;
  link_from_to[0][1]->duplicity = 4096;
  link_from_to[1][0]->reliability = 57344;
  link_from_to[1][0]->duplicity = 4096;

  /* Two round trips = 40 instants = 8 messages */
  praef_system_conf_linear_ack_max_xmit(sys[0], 8);
  praef_system_conf_linear_ack_max_xmit(sys[1], 8);

  praef_system_bootstrap(sys[0]);
  activity[0] = ts_active;
  advance(100);
  praef_system_connect(sys[1], net_id[0]);
  activity[0] = ts_semiactive;
  activity[1] = ts_idle;
  advance(3000);
  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert_int_eq(praef_ss_ok, status[1]);
  activity[1] = ts_active;
  advance(100);
  activity[0] = activity[1] = ts_idle;
  advance(256);

  ck_assert_int_eq(praef_ss_ok, status[0]);
  ck_assert_int_eq(praef_ss_ok, status[1]);
  ck_assert_int_eq(object_at(0,0)->state[750], object_at(1,0)->state[750]);
  ck_assert_int_eq(object_at(0,1)->state[750], object_at(1,1)->state[750]);
}
