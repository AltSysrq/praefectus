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

#include <libpraefectus/virtual-bus.h>

static praef_virtual_network* network;

defsuite(libpraefectus_virtual_bus);

defsetup {
  network = praef_virtual_network_new();
}

defteardown {
  praef_virtual_network_delete(network);
}

#define MB(bus) (praef_virtual_bus_mb(bus))
#define BUS(name) praef_virtual_bus* name =     \
    praef_virtual_network_create_node(network)
#define LINK(from,to) praef_virtual_network_link* from##_##to \
  __attribute__((unused)) =                                   \
    praef_virtual_bus_link(from, to)
#define LINKS(a,b) LINK(a,b); LINK(b,a)
#define CALL(this,meth,...) ((*MB(this)->meth)(MB(this), __VA_ARGS__))
#define OPEN(from,to) \
  ck_assert(CALL((from), create_route, praef_virtual_bus_address(to)))
#define CLOSE(from,to) \
  ck_assert(CALL((from), delete_route, praef_virtual_bus_address(to)))
#define SEND(from,to,meth,value)                        \
  do {                                                  \
    unsigned _send_message = (value);                   \
    CALL((from), meth, praef_virtual_bus_address(to),   \
         &_send_message, sizeof(_send_message));        \
  } while (0)

#define BCAST(from, value)                              \
  do {                                                  \
    unsigned _send_message = (value);                   \
    CALL((from), broadcast, &_send_message,             \
         sizeof(_send_message));                        \
  } while (0)

#define RECV(dst,bus)                                   \
  ((*MB(bus)->recv)(dst, sizeof(*(dst)), MB(bus)))

#define MUST_RECV(dst, bus)                     \
  ck_assert(sizeof(*dst) == RECV((dst), (bus)))

#define MUST_RECV_EXACTLY(value, bus)           \
  do {                                          \
    unsigned _mre_actual;                       \
    MUST_RECV(&_mre_actual, (bus));             \
    ck_assert_int_eq(_mre_actual, (value));     \
  } while (0)

#define EMPTY(bus)                              \
  do {                                          \
    unsigned _empty_dst;                        \
    ck_assert(!RECV(&_empty_dst, (bus)));       \
  } while (0)

#define ADVANCE(amt) praef_virtual_network_advance(network, (amt))

deftest(can_send_packets_over_ideal_network) {
  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  EMPTY(dst);
  SEND(src, dst, triangular_unicast, 42);
  ADVANCE(1);
  MUST_RECV_EXACTLY(42, dst);
  EMPTY(dst);
  EMPTY(src);
}

deftest(nat_simulation_blocks_incoming_packets) {
  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  ADVANCE(1);
  SEND(src, dst, unicast, 42);
  ADVANCE(1);
  EMPTY(dst);
}

deftest(nat_block_can_be_opened) {
  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  ADVANCE(1);
  OPEN(dst, src);
  SEND(src, dst, unicast, 42);
  ADVANCE(1);
  MUST_RECV_EXACTLY(42, dst);
  EMPTY(dst);
}

deftest(transmission_temporarily_opens_nat) {
  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  src_dst->firewall_grace_period = 5;
  ADVANCE(10);
  /* dst talks to src. This gets dropped, since src's NAT is still closed, but
   * it temporarily opens dst's NAT.
   */
  SEND(dst, src, unicast, 5);
  ADVANCE(1);
  EMPTY(src);
  /* Passes through opening */
  SEND(src, dst, unicast, 42);
  ADVANCE(1);
  MUST_RECV_EXACTLY(42, dst);
  EMPTY(dst);
  ADVANCE(10);
  /* Hole in NAT is closed */
  SEND(src, dst, unicast, 6);
  ADVANCE(1);
  EMPTY(dst);
}

deftest(can_delete_routes) {
  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  src_dst->firewall_grace_period = 5;
  ADVANCE(10);
  OPEN(dst, src);
  ADVANCE(10);
  CLOSE(dst, src);
  ADVANCE(1);
  /* Hole in NAT has not yet closed */
  SEND(src, dst, unicast, 42);
  ADVANCE(1);
  MUST_RECV_EXACTLY(42, dst);
  ADVANCE(5);
  /* Hole in NAT closes */
  SEND(src, dst, unicast, 6);
  ADVANCE(1);
  EMPTY(dst);
}

deftest(will_randomly_lose) {
  unsigned i, sum_received = 0;
  unsigned received[256] = { 0 };
  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  src_dst->reliability = 0x7FFF; /* 50% packet loss */
  OPEN(dst, src);
  ADVANCE(1);

  for (i = 0; i < 256; ++i)
    SEND(src, dst, unicast, i);

  ADVANCE(1);

  while (RECV(&i, dst)) {
    ck_assert(i < 256);
    ck_assert(!received[i]);
    ++received[i];
    ++sum_received;
  }

  ck_assert_int_gt(sum_received, 0);
  ck_assert_int_lt(sum_received, 256);
}

deftest(will_randomly_duplicate) {
  unsigned i, sum_received = 0, max_duplications = 0;
  unsigned received[256] = { 0 };
  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  src_dst->duplicity = 0x7FFF; /* 50% packet duplication */
  OPEN(dst, src);
  ADVANCE(1);

  for (i = 0; i < 256; ++i)
    SEND(src, dst, unicast, i);

  ADVANCE(1);

  while (RECV(&i, dst)) {
    ck_assert(i < 256);
    ++sum_received;
    ++received[i];
    if (received[i] > max_duplications)
      max_duplications = received[i];
  }

  /* All packets are received at least once */
  for (i = 0; i < 256; ++i)
    ck_assert_int_ne(0, received[i]);

  /* At least one packet underwent duplication */
  ck_assert_int_gt(sum_received, 256);
  /* Some duplicates themselves get duplicated */
  ck_assert_int_gt(max_duplications, 2);
}

deftest(simulates_constant_latency) {
  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  src_dst->base_latency = 5;
  OPEN(dst, src);
  ADVANCE(1);

  SEND(src, dst, unicast, 42);
  ADVANCE(1);
  /* Still in-flight */
  EMPTY(dst);
  ADVANCE(4);
  /* Arrives */
  MUST_RECV_EXACTLY(42, dst);
  EMPTY(dst);

  SEND(src, dst, unicast, 51);
  /* Completely overshoot the latency */
  ADVANCE(50);
  /* Arrives anyway */
  MUST_RECV_EXACTLY(51, dst);
  EMPTY(dst);
}

deftest(simulates_random_latency) {
  unsigned i, r, t, received[256];
  int is_in_order;

  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  src_dst->base_latency = 5;
  src_dst->variable_latency = 1000;
  OPEN(dst, src);

  for (i = 0; i < 256; ++i)
    SEND(src, dst, unicast, i);

  ADVANCE(1);
  /* Nothing arrives yet */
  EMPTY(dst);

  i = 0;
  ADVANCE(4);
  for (t = 0; t < 1000; ++t) {
    ADVANCE(1);
    while (RECV(&r, dst))
      received[i++] = r;
  }

  EMPTY(dst);
  ck_assert_int_eq(i, 256);

  is_in_order = 1;
  for (i = 1; i < 256 && is_in_order; ++i)
    is_in_order &= received[i] < received[i+1];

  ck_assert(!is_in_order);
}

deftest(can_broadcast) {
  BUS(src);
  BUS(a);
  BUS(b);
  BUS(c);
  BUS(d);
  LINKS(src, a);
  LINKS(src, b);
  LINKS(src, c);
  /* No link to d */

  OPEN(a, src);
  OPEN(b, src);
  OPEN(c, src);

  BCAST(src, 42);
  ADVANCE(1);

  MUST_RECV_EXACTLY(42, a);
  MUST_RECV_EXACTLY(42, b);
  MUST_RECV_EXACTLY(42, c);
  EMPTY(a);
  EMPTY(b);
  EMPTY(c);
  EMPTY(d);
  EMPTY(src);
}

deftest(in_flight_and_inbox_are_not_leaked) {
  BUS(src);
  BUS(dst);
  LINKS(src, dst);

  OPEN(dst, src);
  SEND(src, dst, unicast, 42);
  ADVANCE(1);
  SEND(src, dst, unicast, 53);
}
