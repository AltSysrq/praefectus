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
#include <libpraefectus/stdsys.h>
#include <libpraefectus/messages/PraefMsg.h>
#include <libpraefectus/virtual-bus.h>
#include <libpraefectus/object.h>
#include <libpraefectus/event.h>

#define NUM_VNODES 8
#define STD_LATENCY 16

defsuite(libpraefectus_system_unit);

static praef_std_state state;
static praef_app* app;
static praef_system* sys;
static praef_virtual_network* vnet;
static praef_virtual_bus* sysbus;
static praef_virtual_bus* bus[NUM_VNODES];
static praef_virtual_network_link* link_out[NUM_VNODES];
static praef_virtual_network_link* link_in [NUM_VNODES];
static const PraefNetworkIdentifierPair_t* sys_id;
static const PraefNetworkIdentifierPair_t* net_id[NUM_VNODES];
static praef_signator* signator;
static praef_hlmsg_encoder* rpc_enc, * ur_enc, * cr_enc;
static praef_advisory_serial_number message_serno;

typedef struct {
  praef_object self;
  unsigned char evts[32];
  praef_instant now;
} test_object;
static test_object objects[1+NUM_VNODES];
static unsigned num_objects;

typedef struct {
  praef_event self;
  unsigned which;
} test_event;

static void test_object_step(test_object* this, praef_userdata _) {
  ++this->now;
}

static void test_object_rewind(test_object* this, praef_instant when) {
  unsigned i;

  this->now = when;

  for (i = 0; i < 32; ++i)
    if (this->evts[i] >= this->now)
      this->evts[i] = 0;
}

static void create_node_object(praef_app* app, praef_object_id id) {
  ck_assert_int_le(num_objects, NUM_VNODES+1);

  objects[num_objects].self.step = (praef_object_step_t)test_object_step;
  objects[num_objects].self.rewind = (praef_object_rewind_t)test_object_rewind;
  objects[num_objects].self.id = id;
  memset(objects[num_objects].evts, 0, sizeof(objects[num_objects].evts));
  objects[num_objects].now = 0;

  ck_assert_ptr_eq(objects+num_objects,
                   praef_context_add_object(
                     state.context, (praef_object*)objects+num_objects));
  ++num_objects;
}

static void test_event_apply(test_object* target, const test_event* this,
                             praef_userdata _) {
  ck_assert_int_eq(0, target->evts[this->which]);
  target->evts[this->which] = 1;
}

static praef_event* decode_event(praef_app* app, praef_instant instant,
                                 praef_object_id object,
                                 praef_event_serial_number sn,
                                 const void* data, size_t sz) {
  ck_assert_int_eq(sizeof(signed), sz);

  test_event* evt = malloc(sizeof(test_event));
  evt->self.free = free;
  evt->self.apply = (praef_event_apply_t)test_event_apply;
  evt->self.object = object;
  evt->self.instant = instant;
  evt->self.serial_number = sn;
  evt->which = *(const unsigned*)data;
  return (praef_event*)evt;
}

defsetup {
  unsigned i;

  num_objects = 0;
  message_serno = 0;

  vnet = praef_virtual_network_new();
  sysbus = praef_virtual_network_create_node(vnet);
  sys_id = praef_virtual_bus_address(sysbus);
  for (i = 0; i < NUM_VNODES; ++i) {
    bus[i] = praef_virtual_network_create_node(vnet);
    net_id[i] = praef_virtual_bus_address(bus[i]);
    link_out[i] = praef_virtual_bus_link(bus[i], sysbus);
    link_in [i] = praef_virtual_bus_link(sysbus, bus[i]);
    link_out[i]->firewall_grace_period = 100 * STD_LATENCY;
    link_in [i]->firewall_grace_period = 100 * STD_LATENCY;
  }

  praef_std_state_init(&state);
  app = praef_stdsys_new(&state);
  app->create_node_object = create_node_object;
  app->decode_event = decode_event;
  sys = praef_system_new(app,
                         praef_virtual_bus_mb(sysbus),
                         praef_virtual_bus_address(sysbus),
                         STD_LATENCY,
                         praef_sp_lax,
                         praef_siv_any,
                         praef_snl_any,
                         PRAEF_HLMSG_MTU_MIN+8);
  praef_stdsys_set_system(app, sys);

  signator = praef_signator_new();
  rpc_enc = praef_hlmsg_encoder_new(praef_htf_rpc_type, signator,
                                    NULL, PRAEF_HLMSG_MTU_MIN, 0);
  ur_enc = praef_hlmsg_encoder_new(praef_htf_uncommitted_redistributable,
                                   signator, &message_serno,
                                   PRAEF_HLMSG_MTU_MIN, 0);
  cr_enc = praef_hlmsg_encoder_new(praef_htf_committed_redistributable,
                                   signator, &message_serno,
                                   PRAEF_HLMSG_MTU_MIN, 0);
}

defteardown {
  praef_system_delete(sys);
  praef_stdsys_delete(app);
  praef_std_state_cleanup(&state);
  praef_virtual_network_delete(vnet);
  praef_hlmsg_encoder_delete(rpc_enc);
  praef_hlmsg_encoder_delete(ur_enc);
  praef_hlmsg_encoder_delete(cr_enc);
}

deftest(trivial) {
}
