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

defsuite(libpraefectus_system_wire);

static praef_instant current_instant;
static int debug_receive;

static praef_std_state state;
static praef_app* app;
static praef_system* sys;
static praef_system_status sys_status;
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
                     state.context, (praef_object*)(objects+num_objects)));
  ++num_objects;
}

static void test_event_apply(test_object* target, const test_event* this,
                             praef_userdata _) {
  ck_assert_int_eq(0, target->evts[this->which]);
  target->evts[this->which] = this->self.instant;
}

static praef_event* decode_event(praef_app* app, praef_instant instant,
                                 praef_object_id object,
                                 praef_event_serial_number sn,
                                 const void* data, size_t sz) {
  ck_assert_int_eq(1, sz);

  test_event* evt = malloc(sizeof(test_event));
  evt->self.free = free;
  evt->self.apply = (praef_event_apply_t)test_event_apply;
  evt->self.object = object;
  evt->self.instant = instant;
  evt->self.serial_number = sn;
  evt->which = *(const unsigned char*)data;
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
static void advance(unsigned n) {
  while (n--) {
    praef_virtual_network_advance(vnet, 1);
    sys_status = praef_system_advance(sys, 1);
    ++current_instant;
  }
}

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

    advance(1);

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

  abort();
}

static void watch(unsigned max_steps, void (*f)(PraefMsg_t*)) {
  unsigned n;
  unsigned char buff[1024];
  praef_hlmsg hlmsg = { .data = buff };
  const praef_hlmsg_segment* seg;
  PraefMsg_t* decoded;

  while (max_steps--) {
    advance(1);

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

          (*f)(decoded);
          (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, decoded, 0);
        }
      }
    }
  }
}

#define WATCH(n, msg, body)                             \
  watch((n), lambdav((PraefMsg_t* msg), body))

#include "system-wire-expect-macro.h"

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
  int GLUE(field,ANONYMOUS)(const typeof(VALUE.field)* _value) {        \
    body;                                               \
    return 1;                                           \
  }                                                     \
  if (!GLUE(field,ANONYMOUS)(&VALUE.field)) return 0
#define SUB_OPT(field,body)                             \
  int GLUE(field,ANONYMOUS)(const typeof(VALUE.field)* _value) {        \
    body;                                               \
    return 1;                                           \
  }                                                     \
  if (!GLUE(field,ANONYMOUS)(VALUE.field)) return 0
#define CSUB(field,body) SUB(choice, SUB(field, body))
#define IS(n) WHERE(VALUE == (n))

#define CONSTANTLY(value)                       \
  ({ typeof(value) ANONYMOUS() { return (value); }; ANONYMOUS; })

static praef_node* incarnate(unsigned n) {
  praef_node* node;
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];

  praef_signator_pubkey(pubkey, signator[n]);
  node = praef_node_new(
    sys, 0, 100 + n, net_id[n],
    praef_virtual_bus_mb(sysbus), praef_nd_positive, pubkey);
  ck_assert(praef_system_register_node(sys, node));

  (*BUS(n)->create_route)(BUS(n), sys_id);

  return node;
}

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
  praef_object_id artificial_id;

  praef_system_bootstrap(sys);

  /* Create an artificial node which has the same id that would be produced by
   * virtual node 0 joining.
   */
  praef_signator_pubkey(pubkey, signator[0]);
  praef_signator_pubkey(unrelated_pubkey, signator[2]);
  praef_sha3_init(&sponge);
  praef_keccak_sponge_absorb(&sponge, sys->join.system_salt,
                             sizeof(sys->join.system_salt));
  praef_keccak_sponge_absorb(&sponge, pubkey, sizeof(pubkey));
  artificial_id = praef_keccak_sponge_squeeze_integer(
    &sponge, sizeof(praef_object_id));
  if (artificial_id <= 1) artificial_id = 2;
  artificial = praef_node_new(
    sys, 0, artificial_id,
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

deftest(ignores_join_request_with_invalid_signature) {
  unsigned char pubkey[PRAEF_PUBKEY_SIZE] = { 0 };

  praef_system_bootstrap(sys);
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
  advance(5);
  ck_assert_int_eq(1, num_objects);
}

deftest(ignores_join_request_with_missing_auth) {
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];

  app->is_auth_valid_opt =
    (praef_app_is_auth_valid_t)CONSTANTLY(1);

  praef_signator_pubkey(pubkey, signator[0]);

  praef_system_bootstrap(sys);
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
  advance(5);
  ck_assert_int_eq(1, num_objects);
}

deftest(ignores_join_request_with_invalid_auth) {
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];
  OCTET_STRING_t broken_auth = {
    .buf = (void*)"uiae",
    .size = 4
  };

  app->is_auth_valid_opt =
    (praef_app_is_auth_valid_t)CONSTANTLY(0);

  praef_signator_pubkey(pubkey, signator[0]);

  praef_system_bootstrap(sys);
  SEND(rpc, 0, {
      .present = PraefMsg_PR_joinreq,
      .choice = {
        .joinreq = {
          .publickey = {
            .buf = pubkey,
            .size = PRAEF_PUBKEY_SIZE
          },
          .identifier = *net_id[0],
          .auth = &broken_auth
        }
      }
    });
  advance(5);
  ck_assert_int_eq(1, num_objects);
}

deftest(broadcasts_accept_to_all_nodes) {
  unsigned char pubkey[PRAEF_PUBKEY_SIZE];

  praef_signator_pubkey(pubkey, signator[2]);

  praef_system_bootstrap(sys);
  incarnate(0);
  incarnate(1);
  advance(2);
  SEND(rpc, 2, {
      .present = PraefMsg_PR_joinreq,
      .choice = {
        .joinreq = {
          .publickey = {
            .buf = pubkey,
            .size = PRAEF_PUBKEY_SIZE
          },
          .identifier = *net_id[2],
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
          SUB(present, IS(PraefMsg_PR_accept))),
        MATCHER(
          1,
          SUB(present, IS(PraefMsg_PR_accept))),
        MATCHER(
          2,
          SUB(present, IS(PraefMsg_PR_accept))))));
  ck_assert_int_eq(4, num_objects);
}

deftest(agrees_to_grant_positive_node) {
  praef_system_bootstrap(sys);
  incarnate(0);
  advance(2);

  SEND(cr, 0, {
      .present = PraefMsg_PR_chmod,
      .choice = {
        .chmod = {
          .node = 100,
          .effective = 8,
          .bit = PraefMsgChmod__bit_grant
        }
      }
    });
  EXPECT(
    6,
    EXCHANGE(
      NO_ACTION,
      MATCHERS(
        MATCHER(
          0,
          SUB(present, IS(PraefMsg_PR_chmod));
          CSUB(chmod,
              SUB(node, IS(100));
              SUB(effective, IS(8));
              SUB(bit, IS(PraefMsgChmod__bit_grant)))))));
  advance(5);
  ck_assert(praef_node_is_alive(praef_system_get_node(sys, 100)));
}

static void prepare_populated_htdir(
  PraefMsg_t* dst,
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE],
  PraefHtdirEntry_t* ep[PRAEF_HTDIR_SIZE]
) {
  praef_node* node;
  praef_hash_tree_objref htobj;
  unsigned i;

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i)
    ep[i] = entries+i;

  praef_system_bootstrap(sys);
  node = incarnate(0);

  /* Change the binary version of the public key of the node we control to be
   * the same as the real node, so that we can just bounce the htdir back at
   * it.
   *
   * This doesn't affect signing or verification since the public key has
   * already been converted to integers at this point.
   */
  memcpy(node->pubkey, praef_system_get_node(sys, 1)->pubkey,
         PRAEF_PUBKEY_SIZE);

  /* Manually populate the system's hash tree to guarantee it has at least one
   * subdirectory but that there are also still top-level objects.
   */
  for (i = 0; i < PRAEF_HTDIR_SIZE+1; ++i) {
    htobj.size = sizeof(i);
    htobj.data = &i;
    htobj.instant = 0;
    ck_assert_int_eq(praef_htar_added,
                     praef_hash_tree_add(
                       sys->state.hash_tree, &htobj));
  }

  /* Ensure that the system doesn't spontaneously add more things to the hash
   * tree or send new HtLs messages.
   */
  praef_system_conf_commit_interval(sys, 65536);
  praef_system_conf_ht_root_query_interval(sys, 65536);

  SEND(rpc, 0, {
      .present = PraefMsg_PR_htls,
      .choice = {
        .htls = {
          .snapshot = 0,
          .hash = {
            .buf = (void*)&i,
            .size = 0
          },
          .lownybble = 0
        }
      }
    });
  EXPECT(
    6,
    EXCHANGE(
      NO_ACTION,
      MATCHERS(
        MATCHER(
          0,
          SUB(present, IS(PraefMsg_PR_htdir));
          memcpy(dst, &VALUE, sizeof(PraefMsg_t));
          for (i =  0; i < PRAEF_HTDIR_SIZE; ++i)
            memcpy(entries+i, dst->choice.htdir.entries.list.array[i],
                   sizeof(PraefHtdirEntry_t));
          dst->choice.htdir.entries.list.array = ep))));
}

deftest(htdir_matching_self_results_in_no_queries) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];

  prepare_populated_htdir(&htdir, entries, ep);
  SEND(rpc, 0, htdir);
  WATCH(
    6, msg,
    ck_assert_int_ne(PraefMsg_PR_htls, msg->present);
    ck_assert_int_ne(PraefMsg_PR_htread, msg->present));
}

deftest(htdir_with_extra_object_produces_one_htread) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];
  unsigned i;
  int has_seen_htread = 0;

  prepare_populated_htdir(&htdir, entries, ep);

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    if (PraefHtdirEntry_PR_empty ==
        htdir.choice.htdir.entries.list.array[i]->present) {
      htdir.choice.htdir.entries.list.array[i]->present =
        PraefHtdirEntry_PR_objectid;
      htdir.choice.htdir.entries.list.array[i]->choice.objectid = 65536;
      htdir.choice.htdir.objhash ^= ~0u;
      goto ready;
    }
  }

  ck_abort_msg("Couldn't find an empty htdir slot");

  ready:
  SEND(rpc, 0, htdir);
  WATCH(6, msg,
        ck_assert_int_ne(PraefMsg_PR_htls, msg->present);
        if (PraefMsg_PR_htread == msg->present) {
          ck_assert(!has_seen_htread);
          ck_assert_int_eq(65536, msg->choice.htread.objectid);
          has_seen_htread = 1;
        });

  ck_assert(has_seen_htread);
}

deftest(htdir_with_different_oids_but_same_objhash_produces_no_htread) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];
  unsigned i;

  prepare_populated_htdir(&htdir, entries, ep);
  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i)
    if (PraefHtdirEntry_PR_objectid ==
        htdir.choice.htdir.entries.list.array[i]->present)
      htdir.choice.htdir.entries.list.array[i]->choice.objectid =
        i + 65536;

  SEND(rpc, 0, htdir);
  WATCH(6, msg,
        ck_assert_int_ne(PraefMsg_PR_htls, msg->present);
        ck_assert_int_ne(PraefMsg_PR_htread, msg->present));
}

deftest(htdir_with_different_subdir_produces_one_htls) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];
  unsigned i;
  int has_seen_htls = 0;

  prepare_populated_htdir(&htdir, entries, ep);

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    if (PraefHtdirEntry_PR_subdirsid ==
        htdir.choice.htdir.entries.list.array[i]->present) {
      htdir.choice.htdir.entries.list.array[i]->choice.subdirsid ^= ~0u;
      goto ready;
    }
  }

  ck_abort_msg("Couldn't find an empty htdir slot");

  ready:
  SEND(rpc, 0, htdir);
  WATCH(6, msg,
        ck_assert_int_ne(PraefMsg_PR_htread, msg->present);
        if (PraefMsg_PR_htls == msg->present) {
          ck_assert(!has_seen_htls);
          ck_assert_int_eq(1, msg->choice.htls.hash.size);
          ck_assert_int_eq(i << 4,
                           (unsigned char)msg->choice.htls.hash.buf[0]);
          ck_assert(!msg->choice.htls.lownybble);
          has_seen_htls = 1;
        });

  ck_assert(has_seen_htls);
}

deftest(htdir_with_extra_subdir_produces_one_htls) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];
  unsigned i;
  int has_seen_htls = 0;

  prepare_populated_htdir(&htdir, entries, ep);

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    if (PraefHtdirEntry_PR_empty ==
        htdir.choice.htdir.entries.list.array[i]->present) {
      htdir.choice.htdir.entries.list.array[i]->present =
        PraefHtdirEntry_PR_subdirsid;
      htdir.choice.htdir.entries.list.array[i]->choice.subdirsid = 65536;
      goto ready;
    }
  }

  ck_abort_msg("Couldn't find an empty htdir slot");

  ready:
  SEND(rpc, 0, htdir);
  WATCH(6, msg,
        ck_assert_int_ne(PraefMsg_PR_htread, msg->present);
        if (PraefMsg_PR_htls == msg->present) {
          ck_assert(!has_seen_htls);
          ck_assert_int_eq(1, msg->choice.htls.hash.size);
          ck_assert_int_eq(i << 4,
                           (unsigned char)msg->choice.htls.hash.buf[0]);
          ck_assert(!msg->choice.htls.lownybble);
          has_seen_htls = 1;
        });

  ck_assert(has_seen_htls);
}

deftest(htdir_with_different_objhash_produces_htread_for_each_object) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];
  unsigned i;

  prepare_populated_htdir(&htdir, entries, ep);
  htdir.choice.htdir.objhash ^= ~0u;

  SEND(rpc, 0, htdir);
  WATCH(6, msg,
        unsigned i;
        ck_assert_int_ne(PraefMsg_PR_htls, msg->present);
        if (PraefMsg_PR_htread == msg->present) {
          for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
            if (PraefHtdirEntry_PR_objectid ==
                htdir.choice.htdir.entries.list.array[i]->present &&
                msg->choice.htread.objectid ==
                htdir.choice.htdir.entries.list.array[i]->choice.objectid) {
              htdir.choice.htdir.entries.list.array[i]->present =
                PraefHtdirEntry_PR_empty;
              return;
            }
          }

          ck_abort_msg("Unexpected htread");
        });

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i)
    ck_assert_int_ne(PraefHtdirEntry_PR_objectid,
                     htdir.choice.htdir.entries.list.array[i]->present);
}

deftest(htdir_removing_object_results_in_no_requests) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];
  unsigned i;

  prepare_populated_htdir(&htdir, entries, ep);
  htdir.choice.htdir.objhash ^= ~0u;
  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    if (PraefHtdirEntry_PR_objectid ==
        htdir.choice.htdir.entries.list.array[i]->present) {
      htdir.choice.htdir.entries.list.array[i]->present =
        PraefHtdirEntry_PR_empty;
      goto ready;
    }
  }

  ck_abort_msg("Couldn't find an empty htdir slot");

  ready:
  SEND(rpc, 0, htdir);
  WATCH(6, msg,
        ck_assert_int_ne(PraefMsg_PR_htls, msg->present);
        ck_assert_int_ne(PraefMsg_PR_htread, msg->present));
}

deftest(htdir_removing_subdir_results_in_no_requests) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];
  unsigned i;

  prepare_populated_htdir(&htdir, entries, ep);
  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    if (PraefHtdirEntry_PR_subdirsid ==
        htdir.choice.htdir.entries.list.array[i]->present) {
      htdir.choice.htdir.entries.list.array[i]->present =
        PraefHtdirEntry_PR_empty;
      goto ready;
    }
  }

  ck_abort_msg("Couldn't find an empty htdir slot");

  ready:
  SEND(rpc, 0, htdir);
  WATCH(6, msg,
        ck_assert_int_ne(PraefMsg_PR_htls, msg->present);
        ck_assert_int_ne(PraefMsg_PR_htread, msg->present));
}

deftest(htdir_changing_subdir_to_object_results_in_no_requests) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];
  unsigned i;

  prepare_populated_htdir(&htdir, entries, ep);
  htdir.choice.htdir.objhash ^= ~0u;
  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    if (PraefHtdirEntry_PR_subdirsid ==
        htdir.choice.htdir.entries.list.array[i]->present) {
      htdir.choice.htdir.entries.list.array[i]->present =
        PraefHtdirEntry_PR_objectid;
      goto ready;
    }
  }

  ck_abort_msg("Couldn't find an empty htdir slot");

  ready:
  SEND(rpc, 0, htdir);
  WATCH(6, msg,
        ck_assert_int_ne(PraefMsg_PR_htls, msg->present);
        ck_assert_int_ne(PraefMsg_PR_htread, msg->present));
}

deftest(htdir_changing_object_to_subdir_results_in_htls) {
  PraefMsg_t htdir;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE], * ep[PRAEF_HTDIR_SIZE];
  unsigned i;
  int has_seen_htls = 0;

  prepare_populated_htdir(&htdir, entries, ep);
  htdir.choice.htdir.objhash ^= ~0u;
  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    if (PraefHtdirEntry_PR_objectid ==
        htdir.choice.htdir.entries.list.array[i]->present) {
      htdir.choice.htdir.entries.list.array[i]->present =
        PraefHtdirEntry_PR_subdirsid;
      goto ready;
    }
  }

  ck_abort_msg("Couldn't find an empty htdir slot");

  ready:
  SEND(rpc, 0, htdir);
  WATCH(6, msg,
        ck_assert_int_ne(PraefMsg_PR_htread, msg->present);
        if (PraefMsg_PR_htls == msg->present) {
          ck_assert(!has_seen_htls);
          ck_assert_int_eq(1, msg->choice.htls.hash.size);
          ck_assert_int_eq(i << 4,
                           (unsigned char)msg->choice.htls.hash.buf[0]);
          ck_assert(!msg->choice.htls.lownybble);
          has_seen_htls = 1;
        });

  ck_assert(has_seen_htls);
}

static void gain_grant(unsigned n) {
  SEND(cr, 0, {
      .present = PraefMsg_PR_chmod,
      .choice = {
        .chmod = {
          .node = n + 100,
          .effective = current_instant+5,
          .bit = PraefMsgChmod__bit_grant
        }
      }
    });
  EXPECT(
    5,
    EXCHANGE(
      NO_ACTION,
      MATCHERS(
        MATCHER(
          n,
          SUB(present, IS(PraefMsg_PR_chmod));
          CSUB(chmod,
               SUB(node, IS(n+100));
               SUB(bit, IS(PraefMsgChmod__bit_grant)))))));
  advance(5);
}

deftest(neutralises_duplicated_event) {
  praef_node* node;
  unsigned char evtbuf[1];
  praef_instant instant;

  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  evtbuf[0] = 5;
  instant = current_instant;
  SEND(cr, 0, {
      .present = PraefMsg_PR_appevt,
      .choice = {
        .appevt = {
          .serialnumber = 42,
          .data.buf = evtbuf,
          .data.size = 1
        }
      }
    });
  EXPECT(
    6,
    EXCHANGE(
      NO_ACTION,
      MATCHERS(
        MATCHER(
          0,
          SUB(present, IS(PraefMsg_PR_vote))))));
  advance(2);

  ck_assert_int_eq(praef_nd_positive, node->disposition);
  ck_assert(objects[1].evts[5]);

  current_instant = instant;
  evtbuf[0] = 6;
  SEND(cr, 0, {
      .present = PraefMsg_PR_appevt,
      .choice = {
        .appevt = {
          .serialnumber = 42,
          .data.buf = evtbuf,
          .data.size = 1
        }
      }
    });
  advance(7);

  ck_assert_int_eq(praef_nd_negative, node->disposition);
  ck_assert(!objects[1].evts[5]);
  ck_assert(!objects[1].evts[6]);
}

deftest(discards_hlmsg_with_invalid_chmod) {
  PraefMsg_t msg;
  praef_node* node;
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg encoded = { .data = data, .size = sizeof(data) };
  praef_message_bus* mb = praef_virtual_bus_mb(bus[0]);

  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  praef_hlmsg_encoder_set_now(cr_enc[0], current_instant);

  /* Encode a valid chmod killing the bootstrap node and a chmod with an
   * unknown node id. This should cause the entire packet to be ignored, such
   * that the bootstrap node does not get kicked.
   */
  memset(&msg, 0, sizeof(msg));
  msg.present = PraefMsg_PR_chmod;
  msg.choice.chmod.node = 1;
  msg.choice.chmod.bit = PraefMsgChmod__bit_deny;
  msg.choice.chmod.effective = current_instant + 5;
  praef_hlmsg_encoder_append(&encoded, cr_enc[0], &msg);
  msg.choice.chmod.node = 42;
  praef_hlmsg_encoder_append(&encoded, cr_enc[0], &msg);
  praef_hlmsg_encoder_flush(&encoded, cr_enc[0]);
  (*mb->triangular_unicast)(mb, sys_id, encoded.data, encoded.size-1);

  advance(6);
  ck_assert_int_eq(praef_ss_ok, sys_status);
  /* Ensure disposition of the fake node did not become negative, which would
   * indicate that the test broke, "passing" by virtue of manufacturing an
   * invalid chmod.
   */
  ck_assert_int_eq(praef_nd_positive, node->disposition);
}

deftest(disposition_becomes_negative_on_vote_too_far_into_future) {
  praef_node* node;

  praef_system_conf_max_event_vote_offset(sys, 5);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  current_instant = 5;
  SEND(cr, 0, {
      .present = PraefMsg_PR_vote,
      .choice = {
        .vote = {
          .node = 1,
          .instant = 11,
          .serialnumber = 0
        }
      }
    });
  advance(1);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(disposition_becomes_negative_on_vote_too_far_into_past) {
  praef_node* node;

  praef_system_conf_max_event_vote_offset(sys, 5);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  current_instant = 11;
  SEND(cr, 0, {
      .present = PraefMsg_PR_vote,
      .choice = {
        .vote = {
          .node = 1,
          .instant = 5,
          .serialnumber = 0
        }
      }
    });
  advance(1);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(disposition_becomes_negative_on_duplicate_vote) {
  praef_node* node;

  praef_system_conf_max_event_vote_offset(sys, 5);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  current_instant = 5;
  SEND(cr, 0, {
      .present = PraefMsg_PR_vote,
      .choice = {
        .vote = {
          .node = 1,
          .instant = 5,
          .serialnumber = 0
        }
      }
    });
  current_instant = 6;
  SEND(cr, 0, {
      .present = PraefMsg_PR_vote,
      .choice = {
        .vote = {
          .node = 1,
          .instant = 5,
          .serialnumber = 0
        }
      }
    });
  advance(1);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(disposition_becomes_negative_on_commit_with_invalid_bounds) {
  praef_node* node;

  praef_system_bootstrap(sys);
  node = incarnate(0);

  current_instant = 2;
  SEND(ur, 0, {
      .present = PraefMsg_PR_commit,
      .choice = {
        .commit = {
          .start = 3,
          .hash = {
            .buf = /* arbitrary memory */ (void*)node,
            .size = PRAEF_HASH_SIZE
          }
        }
      }
    });
  advance(1);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(disposition_becomes_negative_on_overlapping_commits) {
  praef_node* node;

  praef_system_bootstrap(sys);
  node = incarnate(0);

  current_instant = 2;
  SEND(ur, 0, {
      .present = PraefMsg_PR_commit,
      .choice = {
        .commit = {
          .start = 0,
          .hash = {
            .buf = /* arbitrary memory */ (void*)node,
            .size = PRAEF_HASH_SIZE
          }
        }
      }
    });
  current_instant = 3;
  SEND(ur, 0, {
      .present = PraefMsg_PR_commit,
      .choice = {
        .commit = {
          .start = 2,
          .hash = {
            .buf = /* arbitrary memory */ (void*)node,
            .size = PRAEF_HASH_SIZE
          }
        }
      }
    });
  advance(2);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(disposition_becomes_negative_on_chmod_in_past) {
  praef_node* node;

  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  current_instant = 2;
  SEND(cr, 0, {
      .present = PraefMsg_PR_chmod,
      .choice = {
        .chmod = {
          .node = 1,
          .effective = 1,
          .bit = PraefMsgChmod__bit_deny
        }
      }
    });
  advance(2);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(disposition_becomes_negative_on_chmod_too_far_in_future) {
  praef_node* node;

  praef_system_conf_vote_chmod_offset(sys, 5);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  current_instant = 2;
  SEND(cr, 0, {
      .present = PraefMsg_PR_chmod,
      .choice = {
        .chmod = {
          .node = 1,
          .effective = 8,
          .bit = PraefMsgChmod__bit_deny
        }
      }
    });
  advance(2);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(disposition_becomes_negative_on_deny_chmod_for_self) {
  praef_node* node;

  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  current_instant = 2;
  SEND(cr, 0, {
      .present = PraefMsg_PR_chmod,
      .choice = {
        .chmod = {
          .node = 100,
          .effective = 4,
          .bit = PraefMsgChmod__bit_deny
        }
      }
    });
  advance(2);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(all_functions_are_present) {
  /* This isn't really a "unit test" per se. It just ensures that all the
   * functions defined to exist in system.h actually do exist.
   */
  praef_system_get_clock(sys);
  praef_system_conf_clock_obsolescence_interval(sys, 5);
  praef_system_conf_clock_tolerance(sys, 5);
  praef_system_conf_commit_interval(sys, 5);
  praef_system_conf_max_commit_lag(sys, 5);
  praef_system_conf_max_validated_lag(sys, 5);
  praef_system_conf_commit_lag_laxness(sys, 5);
  praef_system_conf_self_commit_lag_compensation(sys, 2, 4);
  praef_system_conf_public_visibility_lag(sys, 5);
  praef_system_conf_stability_wait(sys, 5);
  praef_system_conf_join_tree_query_interval(sys, 5);
  praef_system_conf_accept_interval(sys, 5);
  praef_system_conf_max_live_nodes(sys, 5);
  praef_system_conf_ht_range_max(sys, 5);
  praef_system_conf_ht_range_query_interval(sys, 5);
  praef_system_conf_ht_scan_redundancy(sys, 5);
  praef_system_conf_ht_scan_concurrency(sys, 5);
  praef_system_conf_ht_max_scan_tries(sys, 5);
  praef_system_conf_ht_snapshot_interval(sys, 5);
  praef_system_conf_ht_num_snapshots(sys, 5);
  praef_system_conf_ht_root_query_interval(sys, 5);
  praef_system_conf_ht_root_query_offset(sys, 5);
  praef_system_conf_ungranted_route_interval(sys, 5);
  praef_system_conf_granted_route_interval(sys, 5);
  praef_system_conf_ping_interval(sys, 5);
  praef_system_conf_max_pong_silence(sys, 5);
  praef_system_conf_route_kill_delay(sys, 5);
  praef_system_conf_propose_grant_interval(sys, 5);
  praef_system_conf_vote_deny_interval(sys, 5);
  praef_system_conf_vote_chmod_offset(sys, 5);
  praef_system_conf_grace_period(sys, 5);
  praef_system_conf_direct_ack_interval(sys, 5);
  praef_system_conf_indirect_ack_interval(sys, 5);
  praef_system_conf_linear_ack_interval(sys, 5);
  praef_system_conf_linear_ack_max_xmit(sys, 5);
  praef_system_conf_max_advance_per_frame(sys, 5);
  praef_system_conf_max_event_vote_offset(sys, 5);
}

deftest(disposition_becomes_negative_on_invalid_join_accept) {
  praef_node* node;

  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  SEND(ur, 0, {
      .present = PraefMsg_PR_accept,
      .choice = {
        .accept = {
          .instant = 42,
          .signature = {
            .buf = /* arbitrary */ (void*)node,
            .size = PRAEF_SIGNATURE_SIZE
          },
          .request = {
            .publickey = {
              .buf = /* arbitrary */ (void*)node,
              .size = PRAEF_PUBKEY_SIZE
            },
            .identifier = {
              .internet = NULL,
              .intranet = {
                .port = 1234,
                .address = {
                  .present = PraefIpAddress_PR_ipv4,
                  .choice = {
                    .ipv4 = {
                      .buf = /* arbitrary */ (void*)node,
                      .size = 4
                    }
                  }
                }
              }
            },
            .auth = NULL
          }
        }
      }
    });
  advance(2);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(get_netinfos_with_unacceptable_net_ids_are_ignored) {
  sys->ip_version = praef_siv_6only;

  SEND(rpc, 0, {
      .present = PraefMsg_PR_getnetinfo,
      .choice = {
        .getnetinfo = {
          .retaddr = *net_id[0]
        }
      }
    });
  WATCH(4, _, ck_abort_msg("Unexpected message received"));
}

static void create_valid_join_accept(
  PraefMsg_t* dst,
  unsigned char signature[PRAEF_SIGNATURE_SIZE],
  unsigned char pubkey[PRAEF_PUBKEY_SIZE],
  unsigned for_node
) {
  unsigned char data[PRAEF_HLMSG_MTU_MIN+1];
  praef_hlmsg jrm = { .data = data, .size = sizeof(data) };
  PraefMsg_t jr;

  praef_signator_pubkey(pubkey, signator[for_node]);

  memset(&jr, 0, sizeof(jr));
  jr.present = PraefMsg_PR_joinreq;
  jr.choice.joinreq.publickey.buf = pubkey;
  jr.choice.joinreq.publickey.size = PRAEF_PUBKEY_SIZE;
  jr.choice.joinreq.identifier = *net_id[for_node];
  praef_hlmsg_encoder_singleton(&jrm, rpc_enc[for_node], &jr);

  memcpy(signature, praef_hlmsg_signature(&jrm),
         PRAEF_SIGNATURE_SIZE);

  memset(dst, 0, sizeof(PraefMsg_t));
  dst->present = PraefMsg_PR_accept;
  dst->choice.accept.request = jr.choice.joinreq;
  dst->choice.accept.instant = praef_hlmsg_instant(&jrm);
  dst->choice.accept.signature.buf = signature;
  dst->choice.accept.signature.size = PRAEF_SIGNATURE_SIZE;
}

deftest(correctly_handles_duplicate_accepts) {
  praef_node* na, * nb, * node;
  unsigned char pubkey[PRAEF_PUBKEY_SIZE], signature[PRAEF_SIGNATURE_SIZE];
  PraefMsg_t accept;
  unsigned count;

  create_valid_join_accept(&accept, signature, pubkey, 2);
  praef_system_bootstrap(sys);
  na = incarnate(0);
  nb = incarnate(1);

  SEND(ur, 0, accept);
  SEND(ur, 1, accept);
  advance(2);

  ck_assert_int_ne(praef_nd_negative, na->disposition);
  ck_assert_int_ne(praef_nd_negative, nb->disposition);

  count = 0;
  RB_FOREACH(node, praef_node_map, &sys->nodes) ++count;
  ck_assert_int_eq(4, count);
}

deftest(join_accept_with_ipv4_addr_on_6only_is_invalid) {
  praef_node* node;
  unsigned char pubkey[PRAEF_PUBKEY_SIZE], signature[PRAEF_SIGNATURE_SIZE];
  PraefMsg_t accept;

  create_valid_join_accept(&accept, signature, pubkey, 1);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  sys->ip_version = praef_siv_6only;

  SEND(ur, 0, accept);
  advance(2);

  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(join_accept_with_ipv6_addr_on_4only_is_invalid) {
  praef_node* node;
  unsigned char pubkey[PRAEF_PUBKEY_SIZE], signature[PRAEF_SIGNATURE_SIZE];
  PraefMsg_t accept;
  PraefNetworkIdentifierPair_t net_id6 = {
    .internet = NULL,
    .intranet = {
      .port = 1234,
      .address = {
        .present = PraefIpAddress_PR_ipv6,
        .choice = {
          .ipv6 = {
            .buf = /* arbitrary */ (void*)&net_id6,
            .size = 16
          }
        }
      }
    }
  };

  net_id[1] = &net_id6;
  create_valid_join_accept(&accept, signature, pubkey, 1);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  sys->ip_version = praef_siv_4only;

  SEND(ur, 0, accept);
  advance(2);

  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(join_accept_with_local_addr_on_global_is_invalid) {
  praef_node* node;
  unsigned char pubkey[PRAEF_PUBKEY_SIZE], signature[PRAEF_SIGNATURE_SIZE];
  PraefMsg_t accept;

  create_valid_join_accept(&accept, signature, pubkey, 1);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  sys->net_locality = praef_snl_global;

  SEND(ur, 0, accept);
  advance(2);

  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(join_accept_with_global_addr_on_local_is_invalid) {
  praef_node* node;
  unsigned char pubkey[PRAEF_PUBKEY_SIZE], signature[PRAEF_SIGNATURE_SIZE];
  PraefMsg_t accept;
  PraefNetworkIdentifierPair_t global = *net_id[1];

  global.internet = &global.intranet;
  net_id[1] = &global;

  create_valid_join_accept(&accept, signature, pubkey, 1);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  sys->net_locality = praef_snl_local;

  SEND(ur, 0, accept);
  advance(2);

  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(join_accept_with_bootstrap_pubkey_is_invalid) {
  praef_node* node;
  unsigned char pubkey[PRAEF_PUBKEY_SIZE], signature[PRAEF_SIGNATURE_SIZE];
  PraefMsg_t accept;

  create_valid_join_accept(&accept, signature, pubkey, 1);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  /* Change the system's memory of the bootstrap public key to what we're about
   * to send.
   */
  memcpy(praef_system_get_node(sys, 1)->pubkey,
         pubkey, PRAEF_PUBKEY_SIZE);

  SEND(ur, 0, accept);
  advance(2);

  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(disposition_becomes_negative_if_pong_silence_exceeded) {
  praef_node* node;

  praef_system_conf_max_pong_silence(sys, 2);
  praef_system_bootstrap(sys);
  node = incarnate(0);
  advance(3);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(route_to_node_destroyed_after_route_kill_delay) {
  praef_node* node;

  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  praef_system_conf_route_kill_delay(sys, 2);
  praef_system_conf_vote_deny_interval(sys, 1);
  praef_system_conf_vote_chmod_offset(sys, 2);
  praef_node_negative(node, "Forced by test");
  advance(7);

  ck_assert_ptr_eq(NULL, node->router.cr_mq);
}

deftest(disposition_becomes_negative_on_invalid_event) {
  praef_node* node;

  praef_system_bootstrap(sys);
  node = incarnate(0);
  gain_grant(0);

  app->decode_event = (praef_app_decode_event_t)
    lambda((a), int a, 0*a);

  SEND(cr, 0, {
      .present = PraefMsg_PR_appevt,
      .choice = {
        .appevt = {
          .serialnumber = 42,
          .data = {
            .buf = /* arbitrary */ (void*)node,
            .size = 42
          }
        }
      }
    });
  advance(2);
  ck_assert_int_eq(praef_nd_negative, node->disposition);
}

deftest(appuni_sent_only_to_live_nodes) {
  praef_node* node;
  PraefMsg_t msg = {
    .present = PraefMsg_PR_appuni,
    .choice = {
      .appuni = {
        .data = {
          .buf = (void*)"hello world",
          .size = 12
        }
      }
    }
  };
  int has_seen_appuni = 0;

  void expect_no_appuni() {
    ck_abort_msg("Unexpected appuni");
  }

  void expect_one_appuni(
    praef_app* app, praef_object_id node_id,
    praef_instant instant,
    const void* data,
    size_t sz
  ) {
    ck_assert(!has_seen_appuni);
    ck_assert_int_eq(12, sz);
    ck_assert(!strcmp(data, "hello world"));
    ck_assert_int_eq(100, node_id);
    has_seen_appuni = 1;
  }

  praef_system_bootstrap(sys);
  node = incarnate(0);

  app->recv_unicast_opt = (praef_app_recv_unicast_t)expect_no_appuni;
  SEND(rpc, 0, msg);
  advance(2);

  gain_grant(0);
  app->recv_unicast_opt = expect_one_appuni;
  SEND(rpc, 0, msg);
  advance(2);
  ck_assert(has_seen_appuni);

  praef_system_conf_vote_deny_interval(sys, 1);
  praef_system_conf_vote_chmod_offset(sys, 2);
  node->disposition = praef_nd_negative;
  advance(4);
  ck_assert(!praef_node_is_alive(node));

  app->recv_unicast_opt = (praef_app_recv_unicast_t)expect_no_appuni;
  SEND(rpc, 0, msg);
  advance(2);
}
