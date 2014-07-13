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
#ifndef LIBPRAEFECTUS_MESSAGE_BUS_H_
#define LIBPRAEFECTUS_MESSAGE_BUS_H_

#include <stdlib.h>

#include "messages/PraefNetworkIdentifierPair.h"
#include "common.h"

/**
 * The message-bus structure abstracts away most aspects of physical network
 * communication, primarily to support testing without needing a live network.
 *
 * A message bus maintains some number of routes leading away from the local
 * host, identified by a NetworkIdentifierPair. It is the responsibility of the
 * message bus to try to ensure that bidirectional communication along these
 * routes is possible.
 */
typedef struct praef_message_bus_s praef_message_bus;

/**
 * Creates a new route which the message bus will hold open. If the route is
 * already present, nothing happens and the call succeeds.
 *
 * Once a route is created with this call, it remains in the message bus until
 * the bus is destroyed or the delete_route method is used to remove it.
 *
 * @return Whether the operation succeeds.
 */
typedef int (*praef_message_bus_create_route_t)(
  praef_message_bus*,
  const PraefNetworkIdentifierPair_t*);
/**
 * Removes a route that the message bus was holding open. This call always
 * succeeds. Note that destroying a route does not mean that no communications
 * will occur over it; rather, it just prevents the message bus from
 * proactively maintaining the ability to communicate.
 *
 * @return Whether such a route had been present before this call.
 */
typedef int (*praef_message_bus_delete_route_t)(
  praef_message_bus*,
  const PraefNetworkIdentifierPair_t*);

/**
 * Sends a message unicast to a remote host. The host does not necessarily have
 * to have an established route.
 *
 * This call is asynchronous; there is no way to know whether the destination
 * received the message.
 *
 * Note that there are two variants of this: Normal unicast, and triangular
 * unicast. Normal unicast can only be used if the sender can expect that the
 * destination does (or shortly will have) an active route to the local
 * host. It is therefore suitable for communications once the local host has
 * joined a system. Triangular routing is to be used if it there is no way for
 * the destination to know about this host, so it sees most of its use for
 * establishing the initial connection to a system.
 *
 * (For example, in a UDP-based message-bus, triangular routing might proxy
 * through a well-known Internet-accessible host that the true destination can
 * be expected to be in communication with, in addition to sending the message
 * directly to the destination.)
 */
typedef void (*praef_message_bus_unicast_t)(
  praef_message_bus*,
  const PraefNetworkIdentifierPair_t*,
  const void*, size_t);
/**
 * Sends a message to all routes.
 *
 * This call is asynchronous; there is no way to know whether the destinations
 * receive the message.
 */
typedef void (*praef_message_bus_broadcast_t)(
  praef_message_bus*,
  const void*, size_t);

/**
 * Returns the instant at which the bus last considered the route to the given
 * remote host healthy. If there is no such route, returns 0.
 */
typedef praef_instant (*praef_message_bus_last_recv_t)(
  praef_message_bus*,
  const PraefNetworkIdentifierPair_t*);

/**
 * Pulls an incoming message from the message bus.
 *
 * Each call will read a different message from the bus, so messages will be
 * seen only once via this call. However, identical message deliveries may
 * result in the effect of this producing the same message multiple times.
 *
 * @param dst The buffer into which the message contents are copied.
 * @param sz The maximum size of the destination buffer.
 * @return The number of bytes written into dst. A return value of 0 indicates
 * that no new messages are available.
 */
typedef size_t (*praef_message_bus_recv_t)(
  void* dst, size_t sz,
  praef_message_bus*);

struct praef_message_bus_s {
  praef_message_bus_create_route_t create_route;
  praef_message_bus_delete_route_t delete_route;

  praef_message_bus_unicast_t unicast;
  praef_message_bus_unicast_t triangular_unicast;
  praef_message_bus_broadcast_t broadcast;

  praef_message_bus_last_recv_t last_recv;
  praef_message_bus_recv_t recv;
};

#endif /* LIBPRAEFECTUS_MESSAGE_BUS_H_ */
