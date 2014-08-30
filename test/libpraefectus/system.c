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

#include <libpraefectus/system.h>
#include <libpraefectus/-system.h>
#include <libpraefectus/-system-state.h>
#include <libpraefectus/-system-router.h>
#include <libpraefectus/virtual-bus.h>

defsuite(libpraefectus_system);

static praef_app app[4];
static praef_system* sys[4];
static praef_virtual_network* vnet;
static praef_virtual_bus* bus[4];

/* "Application Events" are encoded as simply a 1-byte array containing the
 * byte 42. The tests only care about the identity of the events themselves.
 */
static const unsigned char encoded_event = 42;

static praef_event* decode_event(
  praef_app* appv, praef_instant instant, praef_object_id node,
  praef_event_serial_number serno,
  const void* data, size_t sz
) {
  praef_event* evt;

  ck_assert_int_eq(1, sz);
  ck_assert_int_eq(42, *(unsigned char*)data);

  evt = calloc(1, sizeof(praef_event));
  evt->instant = instant;
  evt->object = node;
  evt->serial_number = serno;
  evt->free = free;
  return evt;
}

defsetup {
  unsigned i;

  memset(app, 0, sizeof(app));
  for (i = 0; i < 4; ++i) {
    app[i].size = sizeof(praef_app);
    app[i].decode_event = decode_event;
  }

  vnet = praef_virtual_network_new();
  for (i = 0; i < 4; ++i) {
    bus[i] = praef_virtual_network_create_node(vnet);
    sys[i] = praef_system_new(&app[i],
                              praef_virtual_bus_mb(bus[i]),
                              praef_virtual_bus_address(bus[i]),
                              0, praef_sp_lax,
                              PRAEF_HLMSG_MTU_MIN+8);
  }
}

defteardown {
  unsigned i;

  for (i = 0; i < 4; ++i)
    praef_system_delete(sys[i]);

  praef_virtual_network_delete(vnet);
}

deftest(can_send_minimal_events) {
  int has_created_node = 0, has_created_obj = 0;
  int has_received_evt = 0, has_received_vote = 0;
  praef_event* evt_to_free;

  app[0].get_node_grant_bridge = lambda(
    (this, node),
    praef_app* this; praef_object_id node,
    ck_assert_ptr_eq(app, this);
    ck_assert_int_eq(1, node);
    (praef_instant)0);
  app[0].get_node_deny_bridge = lambda(
    (this, node),
    praef_app* this; praef_object_id node,
    ck_assert_ptr_eq(app, this);
    ck_assert_int_eq(1, node);
    (praef_instant)~0);
  app[0].create_node_bridge = lambda(
    (this, node),
    praef_app* this; praef_object_id node,
    ck_assert_ptr_eq(app, this);
    ck_assert_int_eq(1, node);
    ck_assert(!has_created_node);
    has_created_node = 1;
    1);
  app[0].create_node_object = lambdav(
    (praef_app* this, praef_object_id node),
    ck_assert_ptr_eq(app, this);
    ck_assert_int_eq(1, node);
    ck_assert(has_created_node);
    ck_assert(!has_created_obj);
    has_created_obj = 1);
  app[0].insert_event_bridge = lambdav(
    (praef_app* this, praef_event* evt),
    ck_assert_ptr_eq(app, this);
    ck_assert(!has_received_evt);
    ck_assert_int_eq(1, evt->object);
    ck_assert_int_eq(0, evt->instant);
    ck_assert_int_eq(0, evt->serial_number);
    evt_to_free = evt;
    has_received_evt = 1);
  app[0].vote_bridge = lambdav(
    (praef_app* this, praef_object_id voter,
     praef_object_id object,
     praef_instant instant,
     praef_event_serial_number serno),
    ck_assert_ptr_eq(app, this);
    ck_assert(!has_received_vote);
    ck_assert_int_eq(1, voter);
    ck_assert_int_eq(1, object);
    ck_assert_int_eq(0, instant);
    ck_assert_int_eq(123, serno);
    has_received_vote = 1);
  app[0].advance_bridge = lambdav(
    (praef_app* this, unsigned delta),
    ck_assert_int_eq(10, delta));

  praef_system_bootstrap(sys[0]);
  ck_assert(praef_system_add_event(sys[0], &encoded_event, 1));
  ck_assert(praef_system_vote_event(sys[0], 1, 0, 123));
  ck_assert_int_eq(praef_ss_ok, praef_system_advance(sys[0], 10));
  ck_assert_int_eq(praef_ss_ok, praef_system_advance(sys[0], 10));

  ck_assert(has_created_node);
  ck_assert(has_created_obj);
  ck_assert(has_received_evt);
  ck_assert(has_received_vote);
  free(evt_to_free);
}