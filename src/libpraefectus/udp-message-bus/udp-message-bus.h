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
#ifndef LIBPRAEFECTUS_UDP_MESSAGE_BUS_UDP_MESSAGE_BUS_H_
#define LIBPRAEFECTUS_UDP_MESSAGE_BUS_UDP_MESSAGE_BUS_H_

#include <libpraefectus/message-bus.h>

/**
 * Performs any application-wide initialisation of the network library.
 *
 * On Unix, this does nothing. On Windows, it initialises Winsock.
 *
 * Do not call this if you initialise the platform networking system yourself
 * or via another library.
 *
 * @return 0 on success, or the platform-specific error code on failure.
 */
int praef_umb_application_init(void);

/**
 * The maximum size of the data in a UDP advertisement packet.
 */
#define PRAEF_UMB_ADVERT_MAX_SIZE 1023

/**
 * Specifies the advertisement data for a single other node.
 */
typedef struct {
  /**
   * The id of the system being advertised, for deduplication.
   */
  unsigned sysid;
  /**
   * The number of meaningful bytes in data.
   */
  unsigned data_size;
  /**
   * The application-specific information about the system being advertised in
   * this message.
   */
  unsigned char data[PRAEF_UMB_ADVERT_MAX_SIZE];
} praef_umb_advert;

/**
 * UDP IP version to use for communication.
 */
typedef enum {
  praef_uiv_ipv4, praef_uiv_ipv6
} praef_umb_ip_version;

/**
 * Creates a new UDP message bus.
 *
 * This implementation guesses the local IP address by asking the operating
 * system to "connect" to an arbitrary internet address via a connectionless
 * protocol, and then querying the local address used for this purpose.
 *
 * The initial configuration of the bus will have all listening disabled, no
 * advertisement, vertex, broadcast, and firewall spamming disabled. The
 * majority of applications will want to enable firewall spamming.
 *
 * @param application The name of the host application. This MUST be a valid,
 * non-empty ASN.1 PrintableString of at most 16 characters; violating this
 * constraint has undefined effect.
 * @param version The version of the host application. This MUST be a valid,
 * non-empty ASN.1 PrintableString of at most 8 characters; violating this
 * constraint has undefined effect.
 * @param well_known_ports An array of port numbers (in host byte order) to try
 * to use if at all possible. These are also the ports used for discovery. If
 * no well-known port could be obtained, fallback is to allow the OS to select
 * the port number, but if this occurs, the local host will not be
 * broadcast-discoverable. While putting every port number into the array would
 * allow broadcast to work in all cases, it results in broadcast discovery
 * being a port-scan, which is generally inadvisable. There is rarely any good
 * reason to use more than 4 well-known ports. This value MAY be NULL if
 * num_well_known_ports is zero. On Windows, using well-known ports will
 * generally make the Windows Firewall pop-*under* the application. While this
 * action does not actually require an exception, the application will not be
 * able to use the bus until the user closes the Windows Firewall window in any
 * way (including pressing the Cancel button). Therefore, Internet-only
 * applications should avoid using well-known ports. "Well-known ports" is
 * specific to the application, and is unrelated to the port range between 1
 * and 1024.
 * @param num_well_known_ports The length of the well_known_ports array.
 * @param ip_version The IP version to use on this message bus.
 * @return The allocated message bus, or NULL if insufficient memory was
 * available. If a socket could not be obtained, a non-NULL message bus is
 * still returned, but praef_umb_get_error() will return non-zero immediately.
 */
praef_message_bus* praef_umb_new(const char* application, const char* version,
                                 const unsigned short* well_known_ports,
                                 unsigned num_well_known_ports,
                                 praef_umb_ip_version ip_version);
/**
 * Frees the resources held by the given message bus, allocated by
 * praef_umb_new().
 */
void praef_umb_delete(praef_message_bus*);

/**
 * Sets the advertisement to use in response to discovery and vertex
 * registration.
 *
 * @param advert The advertisement to use. The value is copied into the message
 * bus. NULL disables advertisement of any kind.
 */
void praef_umb_set_advert(praef_message_bus*, const praef_umb_advert* advert);
/**
 * Sets whether the local host will take any action upon receiving a Discover
 * message.
 *
 * @param is_enabled If true and the current advertisement is non-NULL, an
 * Advertisement will be sent in response to any Discover. Otherwise, received
 * Discover messages have no effect.
 */
void praef_umb_set_listen_discover(praef_message_bus*, int is_enabled);
/**
 * Sets whether the local host will take any action upon receiving an
 * Advertisement message, and what it will do in response.
 *
 * @param callback The function to execute upon each valid Advertisement
 * received. If NULL, received Advertisements will be ignored.
 * @param userdata Arbitrary userdata to pass into the callback.
 */
void praef_umb_set_listen_advert(
  praef_message_bus*,
  void (*callback)(praef_userdata, const praef_umb_advert*,
                   const PraefNetworkIdentifierPair_t*),
  praef_userdata userdata);
/**
 * Sets whether a vertex connection will be maintained. Enabling requires
 * praef_umb_lookup_vertex() to have succeeded first.
 *
 * @param is_enabled If true, the local host will use the vertex server to
 * discover its own Internet address, maintain registration of the current
 * advertisement, and perform triangular routing. If false, the Internet
 * address will be unavailable, and triangular routing behaves as normal
 * unicast.
 */
void praef_umb_set_use_vertex(praef_message_bus*, int is_enabled);
/**
 * Sets whether the *whole* network supports broadcast. It is generally safe to
 * set this if only broadcast discovery (as with praef_umb_send_discovery()) is
 * used. Note that nodes not connected to a well-known port cannot receive
 * broadcasts.
 *
 * @param use_broadcast Whether broadcast calls actually broadcast messages. If
 * false, broadcasts are implemented by poor-man's-multicast as a unicast to
 * each and every active route.
 * @return Zero on success, or the platform error code if attempting to enable
 * broadcasting fails.
 */
int praef_umb_set_use_broadcast(praef_message_bus*, int use_broadcast);
/**
 * Whether periodic null packets should be broadcast.
 *
 * This is necessary on some systems, in particular when behind the Windows
 * Firewall, in order to receive unsolicited UDP messages. Internet-only usages
 * which use a vertex server for triangular routing can generally disable
 * this.
 *
 * @param is_enabled Whether to periodically broadcast nothing in particular.
 * @return Zero on success, or the platform error code if attempting to enable
 * broadcasting on the socket fails.
 */
int praef_umb_set_spam_firewall(praef_message_bus*, int is_enabled);

/**
 * Returns the local-only network identifier used by this message bus. This is
 * always non-NULL.
 */
const PraefNetworkIdentifierPair_t*
praef_umb_local_address(praef_message_bus*);
/**
 * Returns the global network identifier for this message bus, *if it is
 * known*. Returns NULL if unknown. Discovering this value requires a
 * functioning vertex connection.
 */
const PraefNetworkIdentifierPair_t*
praef_umb_global_address(praef_message_bus*);

/**
 * Changes the vertex host used by this message bus to the one provided.
 *
 * Failures to this call do not mark the whole bus as failed.
 *
 * @param hostname The vertex hostname, eg, "vertex.exmaple.org"
 * @param port The port number of the vertex server, in host byte order, eg,
 * 12345.
 * @return Zero if successful, non-zero on error. Non-zero values correspond to
 * errno codes on Unix and Winsock error codes on Windows. The actual value of
 * errno is unlikely to correspond to the returned value.
 */
int praef_umb_lookup_vertex(praef_message_bus*, const char* hostname,
                            unsigned short port);
/**
 * Sends a broadcast or vertex discovery via the given message bus, depending
 * on whether vertex is in use.
 *
 * @return Zero if successful, non-zero on error.
 */
int praef_umb_send_discovery(praef_message_bus*);

/**
 * Returns any permanent error condition that has been encountered by the
 * message bus.
 *
 * @return Zero if the message bus is functioning normally. Non-zero values
 * correspond to errno codes on Unix and Winsock error codes on Windows. The
 * actual value of errno is unlikely to correspond to the returned value.
 * Non-zero return values are always permanent.
 */
int praef_umb_get_error(const praef_message_bus*);

/**
 * Returns a human-readable string describing what the message bus was
 * attempting to do when it encountered the error returned by
 * praef_umb_get_error().
 */
const char* praef_umb_get_error_context(const praef_message_bus*);

#endif /* LIBPRAEFECTUS_UDP_MESSAGE_BUS_UDP_MESSAGE_BUS_H_ */
