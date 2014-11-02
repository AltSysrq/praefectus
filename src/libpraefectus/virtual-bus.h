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
#ifndef LIBPRAEFECTUS_VIRTUAL_BUS_H_
#define LIBPRAEFECTUS_VIRTUAL_BUS_H_

#include "message-bus.h"

__BEGIN_DECLS

/**
 * A virtual network simulates an optionally latent and lossy asynchronous
 * network between multiple virtual message busses. This is primarily useful
 * for testing, but is also useful for networkless multi-node systems in some
 * cases.
 *
 * A virtual network object itself is a container for multiple virtual busses,
 * each of which is a single message bus. In order for two virtual busses to
 * exchange messages, they must be linked. Links are associated with certain
 * parameters that can be changed at any time, and are one-way; asynchronous
 * communication behaviours are fully supported.
 *
 * Nodes in a virtual network have intranet IPv4 addresses in the
 * 127.XXX.XXX.XXX range and ports equal to 0, and do not have Internet
 * addresses.
 *
 * Times used with the virtual network do not need to correspond to the same
 * system used for instants elsewhere; for tests, it is often desireable to use
 * a higher-resolution time for the network times to increase the permutation
 * of message orders that may be seen.
 */
typedef struct praef_virtual_network_s praef_virtual_network;
/**
 * A single virtual bus which is part of a virtual network.
 */
typedef struct praef_virtual_bus_s praef_virtual_bus;

/**
 * Details on a single one-way link from one virtual bus to another. Parameters
 * contained may be changed at any time, but changes may not have effect on
 * messages already queued for delivery.
 */
typedef struct {
  /**
   * The guaranteed minimum latency for packets transmitted over this
   * link. Defaults to zero.
   */
  unsigned base_latency;
  /**
   * Maximum value of random latency to add to base_latency for each individual
   * packet. Defaults to zero. If both this and base_latency are zero, the
   * network is effectively zero-latency; packets become available to the other
   * end of the link immediately after the following
   * praef_virtual_network_advance() call.
   */
  unsigned variable_latency;
  /**
   * The time since the *receiving* side of the link has deleted its route
   * going in the opposite direction or sent a packet in the opposite direction
   * (ie, to the *source* side of this link), whichever is later, after which
   * this link becomes inoperable. This simulates certain firewall behaviours
   * and typical NAT behaviour for UDP.
   *
   * Defaults to 0: The receiver must have a route open to the source in order
   * for this link to be operable.
   *
   * Note that triangular routing bypasses this filter.
   */
  unsigned firewall_grace_period;
  /**
   * Indicates the probability that any given packet will survive this link. 0
   * guarantees 100% packet loss; 65535 creates a perflectly reliable
   * network link. The default is 65535.
   */
  unsigned short reliability;
  /**
   * Indicates the probability that any given packet will be duplicated when
   * sent over this link. The duplicates are subject to different latency
   * calculations any may themselves be duplicated or dropped. 0 guarantees
   * that no duplicates will be produced. The default is 0. Setting this to a
   * large value is a Bad Idea.
   */
  unsigned short duplicity;
} praef_virtual_network_link;

/**
 * Allocates a new, empty virtual network.
 *
 * @return The virtual network, or NULL if insufficient memory is available.
 */
praef_virtual_network* praef_virtual_network_new(void);
/**
 * Frees the memory held by the given virtual network, all virtual busses, and
 * all message queues contained therein.
 */
void praef_virtual_network_delete(praef_virtual_network*);

/**
 * Creates and returns a new virtual bus within the given virtual network. The
 * bus begins with no links to any other bus.
 *
 * @return The new bus, or NULL if insufficient memory is available. The bus is
 * owned by the network object.
 */
praef_virtual_bus* praef_virtual_network_create_node(praef_virtual_network*);
/**
 * Creates a link from one virtual bus to another.
 *
 * Both virtual busses MUST be part of the same virtual network.
 *
 * @return Parameters that can be used to control the link, or NULL if the
 * operation fails. The parameters object is owned by the virtual bus.
 */
praef_virtual_network_link* praef_virtual_bus_link(
  praef_virtual_bus* from, praef_virtual_bus* to);
/**
 * Returns the generic message bus object that passes packets through the input
 * virtual bus. The message bus is owned by the virtual bus.
 */
praef_message_bus* praef_virtual_bus_mb(praef_virtual_bus*);
/**
 * Returns a pointer to the network identifier for the given virtual bus.
 */
const PraefNetworkIdentifierPair_t* praef_virtual_bus_address(
  const praef_virtual_bus*);

/**
 * Returns the number of bytes that have been received by the given bus (after
 * packet duplication/dropping/etc).
 */
unsigned long long praef_virtual_bus_bw_in(
  const praef_virtual_bus*);
/**
 * Returns the number of bytes that have been sent by the given bus (before
 * packet duplication/dropping/etc).
 */
unsigned long long praef_virtual_bus_bw_out(
  const praef_virtual_bus*);

/**
 * Advances the given virtual network the given amount of time forward.
 *
 * @return Whether all operations since the last call to advance()
 * succeeded. Certain conditions, such as running out of memory while trying to
 * enqueue a packet, will cause packets to be dropped regardless of the link
 * parameters; in such a case, advance() will return 0.
 */
int praef_virtual_network_advance(praef_virtual_network*, unsigned);

__END_DECLS

#endif /* LIBPRAEFECTUS_VIRTUAL_BUS_H_ */
