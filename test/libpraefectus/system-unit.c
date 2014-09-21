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
#include <libpraefectus/keccak.h>

#define NUM_VNODES 8
#define STD_LATENCY 16

defsuite(libpraefectus_system_unit);

static praef_instant current_instant;
static int debug_receive;

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
static praef_signator* signator[NUM_VNODES];
static praef_hlmsg_encoder* rpc_enc[NUM_VNODES],
                          * ur_enc[NUM_VNODES],
                          * cr_enc[NUM_VNODES];
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

  ck_assert_ptr_eq(NULL,
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
  current_instant = 0;
  debug_receive = 0;

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

    signator[i] = praef_signator_new();
    rpc_enc[i] = praef_hlmsg_encoder_new(praef_htf_rpc_type, signator[i],
                                         &message_serno,
                                         PRAEF_HLMSG_MTU_MIN, 0);
    ur_enc[i] = praef_hlmsg_encoder_new(praef_htf_uncommitted_redistributable,
                                        signator[i], &message_serno,
                                        PRAEF_HLMSG_MTU_MIN, 0);
    cr_enc[i] = praef_hlmsg_encoder_new(praef_htf_committed_redistributable,
                                        signator[i], &message_serno,
                                        PRAEF_HLMSG_MTU_MIN, 0);
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
}

defteardown {
  unsigned i;

  praef_system_delete(sys);
  praef_stdsys_delete(app);
  praef_std_state_cleanup(&state);
  praef_virtual_network_delete(vnet);

  for (i = 0; i < NUM_VNODES; ++i) {
    praef_hlmsg_encoder_delete(rpc_enc[i]);
    praef_hlmsg_encoder_delete(ur_enc[i]);
    praef_hlmsg_encoder_delete(cr_enc[i]);
    praef_signator_delete(signator[i]);
  }
}

/**
 * Tests here generally take the form
 * - setup (mundane C)
 * - conversation (DSL)
 * - verification (mundane C)
 *
 * A conversation consists of one or more exchanges. An exchange starts with an
 * optional initial set of messages to send, followed by some number of
 * expectations. Each expectation consists of one or more functions which match
 * PraefMsg_ts and a function to execute when all have been matched. The test
 * fails if not all expectations in an exchange have been met by a certain
 * time. Received messages which match nothing are ignored. If the
 * debug_receive global is set to true, though, every received message is
 * dumped in XML to stdout.
 *
 * None of this system is reentrant.
 */

static void do_send_message(praef_virtual_bus* vbus,
                            praef_hlmsg_encoder* encoder,
                            const PraefMsg_t* msg) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg encoded = { .data = data, .size = sizeof(data) };
  praef_message_bus* bus = praef_virtual_bus_mb(vbus);

  praef_hlmsg_encoder_set_now(encoder, current_instant);
  praef_hlmsg_encoder_singleton(&encoded, encoder, msg);
  (*bus->triangular_unicast)(bus, sys_id, encoded.data, encoded.size-1);
}

#define SEND(type, from, ...) ({do {                            \
    PraefMsg_t _msg = __VA_ARGS__;                              \
    do_send_message(bus[from], type##_enc[from], &_msg);        \
  } while (0);})

typedef int (*message_matcher)(unsigned received_by, const PraefMsg_t*);
typedef void (*matched_callback)(void);

typedef struct {
  message_matcher matchers[64];
  matched_callback on_match;
} exchange;

#define BUS(n) praef_virtual_bus_mb(bus[n])
static void do_expectation(
  exchange* exchanges,
  unsigned num_exchanges,
  unsigned max_steps
) {
  unsigned i, j, n;
  unsigned char buff[1024];
  praef_hlmsg hlmsg = { .data = buff };
  const praef_hlmsg_segment* seg;
  PraefMsg_t* decoded;
  int complete;

  while (max_steps--) {
    /* See if all exchanges have completed */
    complete = 1;
    for (i = 0; complete && i < num_exchanges; ++i)
      if (exchanges[i].on_match)
        complete = 0;

    if (complete) return;

    ++current_instant;
    praef_virtual_network_advance(vnet, 1);
    praef_system_advance(sys, 1);

    for (n = 0; n < NUM_VNODES; ++n) {
      while ((hlmsg.size = (*BUS(n)->recv)(
                buff, sizeof(buff)-1, BUS(n)))) {
        buff[hlmsg.size++] = 0;

        ck_assert(praef_hlmsg_is_valid(&hlmsg));

        for (seg = praef_hlmsg_first(&hlmsg);
             seg; seg = praef_hlmsg_snext(seg)) {
          decoded = praef_hlmsg_sdec(seg);

          if (debug_receive) {
            printf("\nReceived message by %d "
                   "(instant = %d)\n",
                   n, praef_hlmsg_instant(&hlmsg));
            xer_fprint(stdout, &asn_DEF_PraefMsg, decoded);
          }

          for (i = 0; i < num_exchanges; ++i) {
            if (exchanges[i].on_match) {
              for (j = 0; j < 64; ++j) {
                if (exchanges[i].matchers[j] &&
                    (*exchanges[i].matchers[j])(n, decoded)) {
                  exchanges[i].matchers[j] = NULL;

                  /* See if this exchange is fully matched */
                  for (j = 0; j < 64; ++j)
                    if (exchanges[i].matchers[j])
                      /* Not yet */
                      goto next_segment;

                  /* Fully matched */
                  (*exchanges[i].on_match)();
                  exchanges[i].on_match = NULL;
                }
              }
            }
          }

          next_segment:
          (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, decoded, 0);
        }
      }
    }
  }

  ck_assert(0);
}

#include "system-unit-expect-macro.h"

#define EXCHANGE(...) { __VA_ARGS__ }
#define MATCHERS(...) .matchers = { __VA_ARGS__ }
#define ON_MATCH(body) .on_match = lambdav((void), body)
#define NO_ACTION .on_match = no_action
static void no_action(void) { }
#define MATCHER(from,body) ({                                   \
      int ANONYMOUS(unsigned _n, const PraefMsg_t* _value) {    \
        if (_n != (from)) return 0;                             \
        body;                                                   \
        return 1;                                               \
      }                                                         \
      ANONYMOUS;                                                \
    })
#define VALUE (*_value)
#define WHERE(expr) if (!(expr)) return 0
#define SUB(field,body)                                 \
  int ANONYMOUS(const typeof(VALUE.field)* _value) {    \
    body;                                               \
    return 1;                                           \
  }                                                     \
  if (!ANONYMOUS(&VALUE.field)) return 0
#define SUB_OPT(field,body)                             \
  int ANONYMOUS(const typeof(VALUE.field)* _value) {    \
    body;                                               \
    return 1;                                           \
  }                                                     \
  if (!ANONYMOUS(VALUE.field)) return 0
#define IS(n) WHERE(VALUE == (n))

deftest(get_network_info) {
  praef_system_bootstrap(sys);

  SEND(rpc, 0, {
      .present = PraefMsg_PR_getnetinfo,
      .choice = {
        .getnetinfo = {
          .retaddr = *net_id[0]
        }
      }
    });
  EXPECT(
    3,
    EXCHANGE(
      NO_ACTION,
      MATCHERS(
        MATCHER(
          0,
          SUB(present, IS(PraefMsg_PR_netinfo))))));
}

deftest(detects_chimera_node_on_join) {
  unsigned char pubkey[PRAEF_PUBKEY_SIZE], unrelated_pubkey[PRAEF_PUBKEY_SIZE];
  praef_keccak_sponge sponge;
  praef_node* artificial;

  praef_system_bootstrap(sys);

  /* Create an artifical node which has the same id that would be produced by
   * virtual node 0 joining.
   */
  praef_signator_pubkey(pubkey, signator[0]);
  praef_signator_pubkey(unrelated_pubkey, signator[2]);
  praef_sha3_init(&sponge);
  praef_keccak_sponge_absorb(&sponge, sys->join.system_salt,
                             sizeof(sys->join.system_salt));
  praef_keccak_sponge_absorb(&sponge, pubkey, sizeof(pubkey));
  artificial = praef_node_new(
    sys, 0, praef_keccak_sponge_squeeze_integer(
      &sponge, sizeof(praef_object_id)),
    net_id[2], BUS(2), praef_nd_positive, unrelated_pubkey);
  ck_assert(praef_system_register_node(sys, artificial));

  SEND(rpc, 0, {
      .present = PraefMsg_PR_getnetinfo,
      .choice = {
        .getnetinfo = {
          .retaddr = *net_id[0]
        }
      }
    });
  EXPECT(
    3,
    EXCHANGE(
      NO_ACTION,
      MATCHERS(
        MATCHER(
          0,
          SUB(present, IS(PraefMsg_PR_netinfo))))));
  message_serno = 0;
  SEND(rpc, 0, {
      .present = PraefMsg_PR_joinreq,
      .choice = {
        .joinreq = {
          .publickey = {
            .buf = pubkey,
            .size = PRAEF_PUBKEY_SIZE
          },
          .identifier = *net_id[0],
          .auth = NULL
        }
      }
    });
  EXPECT(
    5,
    EXCHANGE(
      NO_ACTION,
      MATCHERS(
        MATCHER(
          0,
          SUB(present, IS(PraefMsg_PR_accept))))));

  ck_assert_int_eq(praef_nd_negative, artificial->disposition);
}
