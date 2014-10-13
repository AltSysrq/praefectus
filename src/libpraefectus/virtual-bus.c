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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "messages/PraefNetworkIdentifierPair.h"
#include "common.h"
#include "virtual-bus.h"

typedef struct praef_virtual_link_s praef_virtual_link;
struct praef_virtual_link_s {
  praef_virtual_network_link parms;
  praef_virtual_link* reverse;
  praef_virtual_bus* destination;

  /* The last time that the link in this direction was maintained open or had a
   * packet transmitted through it. This is only of interest to the reverse
   * link.
   */
  praef_instant last_xmit;
  int any_xmit;
  int is_route_open;

  SLIST_ENTRY(praef_virtual_link_s) next;
};

typedef struct praef_virtual_bus_message_s {
  TAILQ_ENTRY(praef_virtual_bus_message_s) next;

  size_t sz;
  praef_instant delivery_time;
  unsigned char data[FLEXIBLE_ARRAY_MEMBER];
} praef_virtual_bus_message;

struct praef_virtual_bus_s {
  praef_message_bus message_bus;
  praef_virtual_network* network;

  PraefNetworkIdentifierPair_t network_identifier_pair;
  unsigned char ip_address[4];

  TAILQ_HEAD(,praef_virtual_bus_message_s) inbox;
  TAILQ_HEAD(,praef_virtual_bus_message_s) in_flight;
  SLIST_HEAD(,praef_virtual_link_s) outbound_links;

  SLIST_ENTRY(praef_virtual_bus_s) next;
};

struct praef_virtual_network_s {
  SLIST_HEAD(,praef_virtual_bus_s) busses;
  praef_instant now;
  unsigned num_busses;
  int oom;
};

static praef_virtual_link* praef_virtual_link_new(void);
static void praef_virtual_link_delete(praef_virtual_link*);
static praef_virtual_bus_message* praef_virtual_bus_message_new(
  const void*, size_t);
static void praef_virtual_bus_message_delete(praef_virtual_bus_message*);
static praef_virtual_bus* praef_virtual_bus_new(praef_virtual_network*);
static void praef_virtual_bus_delete(praef_virtual_bus*);
static void praef_virtual_bus_deliver_ready(praef_virtual_bus*);
static int praef_virtual_bus_create_route(
  praef_virtual_bus*, const PraefNetworkIdentifierPair_t*);
static int praef_virtual_bus_delete_route(
  praef_virtual_bus*, const PraefNetworkIdentifierPair_t*);
static void praef_virtual_bus_unicast(
  praef_virtual_bus*, const PraefNetworkIdentifierPair_t*,
  const void*, size_t);
static void praef_virtual_bus_triangular_unicast(
  praef_virtual_bus*, const PraefNetworkIdentifierPair_t*,
  const void*, size_t);
static void praef_virtual_bus_do_unicast(
  praef_virtual_bus*, const PraefNetworkIdentifierPair_t*,
  int is_triangular, const void*, size_t);
static void praef_virtual_bus_broadcast(
  praef_virtual_bus*, const void*, size_t);
static void praef_virtual_bus_send_packet(
  praef_virtual_bus*, praef_virtual_link*,
  int bypass_firewall, const void*, size_t);
static size_t praef_virtual_bus_recv(void*, size_t, praef_virtual_bus*);

static int network_identifiers_equal(const PraefNetworkIdentifierPair_t* a,
                                     const PraefNetworkIdentifierPair_t* b) {
  return !memcmp(a->intranet.address.choice.ipv4.buf,
                 b->intranet.address.choice.ipv6.buf, 4);
}

praef_virtual_network* praef_virtual_network_new(void) {
  praef_virtual_network* this = malloc(sizeof(praef_virtual_network));
  if (!this) return NULL;

  SLIST_INIT(&this->busses);
  this->now = 0;
  this->oom = 0;
  this->num_busses = 0;

  return this;
}

void praef_virtual_network_delete(praef_virtual_network* this) {
  praef_virtual_bus* bus, * tmp;

  SLIST_FOREACH_SAFE(bus, &this->busses, next, tmp)
    praef_virtual_bus_delete(bus);

  free(this);
}

praef_virtual_bus* praef_virtual_network_create_node(
  praef_virtual_network* this
) {
  praef_virtual_bus* bus;

  bus = praef_virtual_bus_new(this);
  if (!bus) return NULL;

  ++this->num_busses;
  SLIST_INSERT_HEAD(&this->busses, bus, next);
  return bus;
}

praef_virtual_network_link* praef_virtual_bus_link(
  praef_virtual_bus* from, praef_virtual_bus* to
) {
  praef_virtual_link* other, * link = praef_virtual_link_new();
  if (!link) return NULL;

  link->destination = to;
  SLIST_INSERT_HEAD(&from->outbound_links, link, next);

  /* See if there is an existing reverse link, and associate the two if so. */
  SLIST_FOREACH(other, &to->outbound_links, next) {
    if (from == other->destination) {
      link->reverse = other;
      other->reverse = link;
      break;
    }
  }

  return &link->parms;
}

praef_message_bus* praef_virtual_bus_mb(praef_virtual_bus* this) {
  return (praef_message_bus*)this;
}

const PraefNetworkIdentifierPair_t* praef_virtual_bus_address(
  const praef_virtual_bus* this
) {
  return &this->network_identifier_pair;
}

int praef_virtual_network_advance(praef_virtual_network* this, unsigned delta) {
  praef_virtual_bus* bus;
  int oom = this->oom;

  this->now += delta;

  SLIST_FOREACH(bus, &this->busses, next)
    praef_virtual_bus_deliver_ready(bus);

  this->oom = 0;
  return !oom;
}

static praef_virtual_link* praef_virtual_link_new(void) {
  praef_virtual_link* this = malloc(sizeof(praef_virtual_link));
  if (!this) return NULL;

  this->parms.base_latency = 0;
  this->parms.variable_latency = 0;
  this->parms.firewall_grace_period = 0;
  this->parms.reliability = 65535;
  this->parms.duplicity = 0;
  this->reverse = NULL;
  this->destination = NULL;
  this->last_xmit = 0;
  this->any_xmit = 0;
  this->is_route_open = 0;
  return this;
}

static void praef_virtual_link_delete(praef_virtual_link* this) {
  free(this);
}

static praef_virtual_bus_message* praef_virtual_bus_message_new(
  const void* data, size_t sz
) {
  praef_virtual_bus_message* this = malloc(
    offsetof(praef_virtual_bus_message,data) + sz);
  if (!this) return NULL;

  this->sz = sz;
  this->delivery_time = ~0;
  memcpy(this->data, data, sz);
  return this;
}

static void praef_virtual_bus_message_delete(praef_virtual_bus_message* this) {
  free(this);
}

static praef_virtual_bus* praef_virtual_bus_new(praef_virtual_network* network) {
  unsigned ipa;
  praef_virtual_bus* this = malloc(sizeof(praef_virtual_bus));
  if (!this) return NULL;

  this->message_bus.create_route =
    (praef_message_bus_create_route_t)praef_virtual_bus_create_route;
  this->message_bus.delete_route =
    (praef_message_bus_delete_route_t)praef_virtual_bus_delete_route;
  this->message_bus.unicast =
    (praef_message_bus_unicast_t)praef_virtual_bus_unicast;
  this->message_bus.triangular_unicast =
    (praef_message_bus_unicast_t)praef_virtual_bus_triangular_unicast;
  this->message_bus.broadcast =
    (praef_message_bus_broadcast_t)praef_virtual_bus_broadcast;
  this->message_bus.recv =
    (praef_message_bus_recv_t)praef_virtual_bus_recv;

  this->network = network;
  ipa = 0x7F000000 + network->num_busses;
  this->ip_address[0] = ipa >> 24;
  this->ip_address[1] = ipa >> 16;
  this->ip_address[2] = ipa >>  8;
  this->ip_address[3] = ipa >>  0;
  this->network_identifier_pair.internet = NULL;
  this->network_identifier_pair.intranet.port = 0;
  this->network_identifier_pair.intranet.address.present =
    PraefIpAddress_PR_ipv4;
  this->network_identifier_pair.intranet.address.choice.ipv4.buf =
    this->ip_address;
  this->network_identifier_pair.intranet.address.choice.ipv4.size = 4;

  TAILQ_INIT(&this->inbox);
  TAILQ_INIT(&this->in_flight);
  SLIST_INIT(&this->outbound_links);

  return this;
}

static void praef_virtual_bus_delete(praef_virtual_bus* this) {
  praef_virtual_link* link, * ltmp;
  praef_virtual_bus_message* msg, * mtmp;

  SLIST_FOREACH_SAFE(link, &this->outbound_links, next, ltmp)
    praef_virtual_link_delete(link);
  TAILQ_FOREACH_SAFE(msg, &this->inbox, next, mtmp)
    praef_virtual_bus_message_delete(msg);
  TAILQ_FOREACH_SAFE(msg, &this->in_flight, next, mtmp)
    praef_virtual_bus_message_delete(msg);

  free(this);
}

static void praef_virtual_bus_deliver_ready(praef_virtual_bus* this) {
  praef_virtual_bus_message* msg, * mtmp;

  TAILQ_FOREACH_SAFE(msg, &this->in_flight, next, mtmp) {
    if (msg->delivery_time <= this->network->now) {
      TAILQ_REMOVE(&this->in_flight, msg, next);
      TAILQ_INSERT_TAIL(&this->inbox, msg, next);
    }
  }
}

static int praef_virtual_bus_create_route(
  praef_virtual_bus* this, const PraefNetworkIdentifierPair_t* id
) {
  praef_virtual_link* link;

  SLIST_FOREACH(link, &this->outbound_links, next) {
    if (network_identifiers_equal(
          id, &link->destination->network_identifier_pair)) {
      link->is_route_open = 1;
    }
  }

  return 1;
}

static int praef_virtual_bus_delete_route(
  praef_virtual_bus* this, const PraefNetworkIdentifierPair_t* id
) {
  praef_virtual_link* link;

  SLIST_FOREACH(link, &this->outbound_links, next) {
    if (network_identifiers_equal(
          id, &link->destination->network_identifier_pair)) {
      if (!link->is_route_open) return 0;

      link->is_route_open = 0;
      link->last_xmit = this->network->now;
      link->any_xmit = 1;
      return 1;
    }
  }

  return 0;
}

static void praef_virtual_bus_unicast(
  praef_virtual_bus* this, const PraefNetworkIdentifierPair_t* dst,
  const void* data, size_t sz
) {
  praef_virtual_bus_do_unicast(this, dst, 0, data, sz);
}

static void praef_virtual_bus_triangular_unicast(
  praef_virtual_bus* this, const PraefNetworkIdentifierPair_t* dst,
  const void* data, size_t sz
) {
  praef_virtual_bus_do_unicast(this, dst, 1, data, sz);
}

static void praef_virtual_bus_do_unicast(
  praef_virtual_bus* this, const PraefNetworkIdentifierPair_t* dst,
  int is_triangular, const void* data, size_t sz
) {
  praef_virtual_link* link;

  SLIST_FOREACH(link, &this->outbound_links, next)
    if (network_identifiers_equal(
          dst, &link->destination->network_identifier_pair))
      break;

  if (!link)
    /* No such link; silently drop packet */
    return;

  /* Regardless of whether the packet will get dropped, the host's firewall
   * will reset the grace period.
   */
  link->last_xmit = this->network->now;
  link->any_xmit = 1;

  praef_virtual_bus_send_packet(this, link, is_triangular, data, sz);
}

static void praef_virtual_bus_broadcast(
  praef_virtual_bus* this, const void* data, size_t sz
) {
  praef_virtual_link* link;

  SLIST_FOREACH(link, &this->outbound_links, next)
    /* Don't update last_xmit of the reverse; depending on the underlying
     * transport mechanism used for broadcast in a real system, broadcasts
     * might not reset the firewall's grace period.
     */
    praef_virtual_bus_send_packet(this, link, 0, data, sz);
}

/* Work around C libraries producing 15-bit random numbers. */
#if RAND_MAX < 65535
#define rand() (rand() | (rand() << 15))
#endif

static void praef_virtual_bus_send_packet(
  praef_virtual_bus* this, praef_virtual_link* link,
  int bypass_firewall, const void* data, size_t sz
) {
  praef_instant reverse_xmit;
  int any_xmit;
  praef_virtual_bus_message* message;

  if (link->reverse) {
    reverse_xmit = link->reverse->last_xmit;
    any_xmit = link->reverse->any_xmit;
  } else {
    reverse_xmit = 0;
    any_xmit = 0;
  }

  if (!bypass_firewall &&
      (!link->reverse || !link->reverse->is_route_open) &&
      (reverse_xmit + link->parms.firewall_grace_period <= this->network->now ||
       !any_xmit))
    /* This packet is not routed so as to bypass firewall/NAT effects, and the
     * destination is not holding the route open and has not transmitted a
     * packet to the sender since the grace period ended. The packet gets
     * dropped silently.
     */
    return;

  /* Check whether the packet should be randomly dropped. */
  if (!link->parms.reliability || (rand() & 0xFFFF) > link->parms.reliability)
    /* Dropped randomly */
    return;

  /* Check whether the packet should be randomly duplicated */
  if ((rand() & 0xFFFF) < link->parms.duplicity)
    praef_virtual_bus_send_packet(this, link, bypass_firewall, data, sz);

  message = praef_virtual_bus_message_new(data, sz);
  if (!message) {
    this->network->oom = 1;
    return;
  }

  message->delivery_time = this->network->now +
    link->parms.base_latency + (rand() % (link->parms.variable_latency+1));

  TAILQ_INSERT_TAIL(&link->destination->in_flight, message, next);
}

static size_t praef_virtual_bus_recv(void* dst, size_t max,
                                     praef_virtual_bus* this) {
  praef_virtual_bus_message* message;
  size_t retval = 0;
  while (!TAILQ_EMPTY(&this->inbox)) {
    message = TAILQ_FIRST(&this->inbox);
    TAILQ_REMOVE(&this->inbox, message, next);

    if (message->sz <= max) {
      retval = message->sz;
      memcpy(dst, message->data, message->sz);
    }

    praef_virtual_bus_message_delete(message);
    if (retval)
      return retval;
  }

  return 0;
}
