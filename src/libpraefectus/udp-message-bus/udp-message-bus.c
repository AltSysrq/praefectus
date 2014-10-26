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

#if HAVE_SYS_SOCKET_H
/* Berkeley Sockets */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

/* Make Berkeley Sockets work like Winsock, since that's easier than the
 * converse.
 */
typedef int praef_umb_socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

static int WSAGetLastError(void) {
  return errno;
}

static int closesocket(praef_umb_socket_t sock) {
  return close(sock);
}

int praef_umb_application_init(void) {
  return 0;
}

#elif HAVE_WINSOCK2_H

/* Winsock */
#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET praef_umb_socket_t;

int praef_umb_application_init(void) {
  WSADATA wsadata;
  return WSAStartup(0x0202 /* 2.2 */, &wsadata);
}

#else

#error "It appears your platform has neither BSD sockets nor Winsock"

#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bsd.h"
#include "udp-message-bus.h"
#include "../../udp-common/PraefUdpMsg.h"

typedef union {
  struct sockaddr_in ipv4;
  struct sockaddr_in6 ipv6;
} combined_sockaddr;

typedef struct praef_umb_route_s {
  const PraefNetworkIdentifierPair_t* netid;

  RB_ENTRY(praef_umb_route_s) tree;
} praef_umb_route;

RB_HEAD(praef_umb_route_tree,praef_umb_route_s);

typedef struct {
  praef_message_bus self;
  praef_umb_socket_t sock;
  int error;
  const char* error_context;
  struct praef_umb_route_tree routes;

  PraefUdpMsgDiscover_t discovery_struct;
  unsigned char discovery_packet[32];
  unsigned discovery_packet_size;

  praef_umb_advert advert;
  int has_advert;

  const unsigned short* well_known_ports;
  unsigned num_well_known_ports;

  praef_umb_ip_version ip_version;

  int has_vertex_address;
  combined_sockaddr vertex_address;

  int listen_discover;
  void (*listen_advert)(praef_userdata, const praef_umb_advert*,
                        const PraefNetworkIdentifierPair_t*);
  praef_userdata listen_advert_userdata;
  int use_vertex;
  int use_broadcast;
  int has_enabled_broadcast_on_sock;
  int spam_firewall;

  unsigned char local_ip[8 * 2], internet_ip[8 * 2];
  PraefNetworkIdentifier_t internet_id;
  PraefNetworkIdentifierPair_t global_netid, local_netid;

  time_t last_vertex_comm;
  time_t last_firewall_spam;
} praef_udp_message_bus;

#define IPV(v, if4, if6)                        \
  ((v) == praef_uiv_ipv4? (if4) : (if6))

#define SASZ(v) IPV((v), sizeof(struct sockaddr_in), \
                    sizeof(struct sockaddr_in6))

static int praef_umb_create_route(praef_message_bus*,
                                  const PraefNetworkIdentifierPair_t*);
static int praef_umb_delete_route(praef_message_bus*,
                                  const PraefNetworkIdentifierPair_t*);
static void praef_umb_unicast(
  praef_message_bus*,
  const PraefNetworkIdentifierPair_t*,
  const void*, size_t);
static void praef_umb_triangular_unicast(
  praef_message_bus*,
  const PraefNetworkIdentifierPair_t*,
  const void*, size_t);
static void praef_umb_broadcast(
  praef_message_bus*,
  const void*, size_t);
static size_t praef_umb_recv(
  void*, size_t, praef_message_bus*);

static int praef_compare_umb_routes(const praef_umb_route*,
                                    const praef_umb_route*);
RB_PROTOTYPE_STATIC(praef_umb_route_tree, praef_umb_route_s, tree,
                    praef_compare_umb_routes)
RB_GENERATE_STATIC(praef_umb_route_tree, praef_umb_route_s,
                   tree, praef_compare_umb_routes)

static size_t praef_umb_per_encode(void* dst, size_t size,
                                   PraefUdpMsg_t* message,
                                   const char* error) {
  asn_enc_rval_t encode_result;

  encode_result = uper_encode_to_buffer(
    &asn_DEF_PraefUdpMsg, message, dst, size);
  if (-1 == encode_result.encoded) {
    fprintf(stderr, error);
    abort();
  }
  return (encode_result.encoded + 7) / 8;
}

praef_message_bus* praef_umb_new(const char* application, const char* version,
                                 const unsigned short* well_known_ports,
                                 unsigned num_well_known_ports,
                                 praef_umb_ip_version ip_version) {
  praef_udp_message_bus* this;
  combined_sockaddr bindaddr, connectaddr;
  PraefUdpMsg_t discover;
  unsigned i;
  socklen_t socklen;

  this = calloc(1, sizeof(praef_udp_message_bus));
  if (!this)
    return NULL;

  this->self.create_route = praef_umb_create_route;
  this->self.delete_route = praef_umb_delete_route;
  this->self.unicast = praef_umb_unicast;
  this->self.triangular_unicast = praef_umb_triangular_unicast;
  this->self.broadcast = praef_umb_broadcast;
  this->self.recv = praef_umb_recv;

  memset(&discover, 0, sizeof(discover));
  discover.present = PraefUdpMsg_PR_discover;
  discover.choice.discover.application.buf = (void*)application;
  discover.choice.discover.application.size = strlen(application);
  discover.choice.discover.version.buf = (void*)version;
  discover.choice.discover.version.size = strlen(version);
  this->discovery_packet_size = praef_umb_per_encode(
    this->discovery_packet, sizeof(this->discovery_packet),
    &discover, "praef_umb_new(): Application or version is invalid\n");
  this->discovery_struct = discover.choice.discover;

  this->error_context = "unknown";
  this->ip_version = ip_version;
  this->well_known_ports = well_known_ports;
  this->num_well_known_ports = num_well_known_ports;
  RB_INIT(&this->routes);
  this->local_netid.intranet.address.present =
    IPV(ip_version, PraefIpAddress_PR_ipv4, PraefIpAddress_PR_ipv6);
  this->local_netid.intranet.address.choice.ipv4.buf = this->local_ip;
  this->local_netid.intranet.address.choice.ipv4.size =
    IPV(ip_version, 4, 16);
  this->global_netid.intranet.address.present =
    IPV(ip_version, PraefIpAddress_PR_ipv4, PraefIpAddress_PR_ipv6);
  this->global_netid.intranet.address.choice.ipv4.buf = this->local_ip;
  this->global_netid.intranet.address.choice.ipv4.size =
    IPV(ip_version, 4, 16);
  this->internet_id.address.present =
    IPV(ip_version, PraefIpAddress_PR_ipv4, PraefIpAddress_PR_ipv6);
  this->internet_id.address.choice.ipv4.buf = this->internet_ip;
  this->internet_id.address.choice.ipv4.size =
    IPV(ip_version, 4, 16);

  this->sock = socket(IPV(ip_version, PF_INET, PF_INET6), SOCK_DGRAM, 0);
  if (INVALID_SOCKET == this->sock) {
    this->error = WSAGetLastError();
    this->error_context = "creating socket";
    goto end;
  }

  /* First, bind on ANY on no particular port, then connect to an Internet
   * address to find out that local address the OS likes to use for talking to
   * non-local Internet hosts.
   */
  memset(&bindaddr, 0, sizeof(bindaddr));
  memset(&connectaddr, 0, sizeof(connectaddr));
  if (praef_uiv_ipv4 == ip_version) {
    bindaddr.ipv4.sin_family = AF_INET;
    bindaddr.ipv4.sin_addr.s_addr = INADDR_ANY;
    bindaddr.ipv4.sin_port = 0;
    connectaddr.ipv4.sin_family = AF_INET;
    connectaddr.ipv4.sin_addr.s_addr = 0x08080808;
    connectaddr.ipv4.sin_port = htons(80);
  } else {
    bindaddr.ipv6.sin6_family = AF_INET6;
    bindaddr.ipv6.sin6_addr = in6addr_any;
    bindaddr.ipv6.sin6_port = 0;
    connectaddr.ipv6.sin6_family = AF_INET6;
    /* There is an amusing amount of confusion as to what `struct in6_addr`
     * actually looks like.
     *
     * MSDN says Winsock provides
     *
     * struct in6_addr {
     *   union {
     *     u_char Byte[16];
     *     u_short Word[8];
     *   } u;
     * };
     *
     * However, Winsock actually provides the following, with a bunch of
     * horrible preprocessor defines to try to make it work like BSD and GNU.
     *
     * struct in6addr {
     *   union {
     *     u_char _S6_u8[16];
     *     u_short _S6_u16[8];
     *     u_long _S6_u32[4];
     *   } _S6_un;
     * };
     *
     * BSD does something similar, but with different names. The CPP
     * adaptations BSD provides are different from the ones MS provides to try
     * to be compatible with BSD.
     *
     * It does at least seem that everyone agrees that sin_addr.s6_addr means
     * "the array of 16" bytes, which is less convenient (and still pretty
     * horrible being a CPP definition), so go with that.
     */
    /* Arbitrary IP address. We won't be sending anything here.
     *
     * This happened to be an IPv6 address of google.com.
     */
    connectaddr.ipv6.sin6_addr.s6_addr[ 0] = 0x26;
    connectaddr.ipv6.sin6_addr.s6_addr[ 1] = 0x07;
    connectaddr.ipv6.sin6_addr.s6_addr[ 2] = 0xF8;
    connectaddr.ipv6.sin6_addr.s6_addr[ 3] = 0xB0;
    connectaddr.ipv6.sin6_addr.s6_addr[ 4] = 0x40;
    connectaddr.ipv6.sin6_addr.s6_addr[ 5] = 0x08;
    /* Zeroes */
    connectaddr.ipv6.sin6_addr.s6_addr[14] = 0x10;
    connectaddr.ipv6.sin6_addr.s6_addr[15] = 0x07;
    connectaddr.ipv6.sin6_port = htons(80);
  }
  if (bind(this->sock, (struct sockaddr*)&bindaddr, SASZ(ip_version))) {
    this->error = WSAGetLastError();
    this->error_context = "initial socket bind";
    goto end;
  }

  if (connect(this->sock, (struct sockaddr*)&connectaddr, SASZ(ip_version))) {
    this->error = WSAGetLastError();
    this->error_context = "creating dummy connection";
    goto end;
  }

  socklen = SASZ(ip_version);
  if (getsockname(this->sock, (struct sockaddr*)&bindaddr, &socklen)) {
    this->error = WSAGetLastError();
    this->error_context = "obtaining local address";
    goto end;
  }

  /* Winsock doesn't clearly define whether there's any way to undo the effect
   * of connect() on a socket, so to be safe, tear the socket down and make a
   * new one.
   */
  closesocket(this->sock);
  this->sock = socket(IPV(ip_version, PF_INET, PF_INET6),
                      SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  if (INVALID_SOCKET == this->sock) {
    this->error = WSAGetLastError();
    this->error_context = "creating socket";
    goto end;
  }

  /* Try to bind to a well known port with the local address */
  for (i = 0; i < num_well_known_ports; ++i) {
    *(IPV(ip_version, &bindaddr.ipv4.sin_port, &bindaddr.ipv6.sin6_port)) =
      htons(well_known_ports[i]);
    if (0 == bind(this->sock, (struct sockaddr*)&bindaddr, SASZ(ip_version)))
      goto socket_bound;
  }

  /* Give up using a well-known port, let the OS choose */
  *(IPV(ip_version, &bindaddr.ipv4.sin_port, &bindaddr.ipv6.sin6_port)) = 0;
  if (bind(this->sock, (struct sockaddr*)&bindaddr, SASZ(ip_version))) {
    this->error = WSAGetLastError();
    this->error_context = "binding socket";
    goto end;
  }

  socklen = SASZ(ip_version);
  if (getsockname(this->sock, (struct sockaddr*)&bindaddr, &socklen)) {
    this->error = WSAGetLastError();
    this->error_context = "obtaining local address during fallback";
    goto end;
  }

  socket_bound:
  /* Populate the local components of the net id, know that we know it */
  if (praef_uiv_ipv4 == ip_version) {
    this->local_netid.intranet.port = this->global_netid.intranet.port =
      ntohs(bindaddr.ipv4.sin_port);
    memcpy(this->local_ip, &bindaddr.ipv4.sin_addr.s_addr, 4);
  } else {
    this->local_netid.intranet.port = this->global_netid.intranet.port =
      ntohs(bindaddr.ipv6.sin6_port);
    memcpy(this->local_ip, bindaddr.ipv6.sin6_addr.s6_addr, 16);
  }

  end:
  return (praef_message_bus*)this;
}

void praef_umb_delete(praef_message_bus* vthis) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  praef_umb_route* route, * tmp;

  for (route = RB_MIN(praef_umb_route_tree, &this->routes);
       route; route = tmp) {
    tmp = RB_NEXT(praef_umb_route_tree, &this->routes, route);
    RB_REMOVE(praef_umb_route_tree, &this->routes, route);
    free(route);
  }

  if (INVALID_SOCKET != this->sock)
    closesocket(this->sock);
  free(this);
}

void praef_umb_set_advert(praef_message_bus* vthis,
                          const praef_umb_advert* advert) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;

  if (advert) {
    this->has_advert = 1;
    memcpy(&this->advert, advert, sizeof(praef_umb_advert));
  } else {
    this->has_advert = 0;
  }
}

void praef_umb_set_listen_discover(praef_message_bus* vthis,
                                   int is_enabled) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;

  this->listen_discover = is_enabled;
}

void praef_umb_set_listen_advert(
  praef_message_bus* vthis,
  void (*callback)(praef_userdata, const praef_umb_advert*,
                   const PraefNetworkIdentifierPair_t*),
  praef_userdata userdata
) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;

  this->listen_advert = callback;
  this->listen_advert_userdata = userdata;
}

void praef_umb_set_use_vertex(praef_message_bus* vthis,
                              int is_enabled) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;

  this->use_vertex = is_enabled;
}

static int praef_umb_ensure_sock_broadcast(
  praef_udp_message_bus* this, int require
) {
  int enable = 1;

  if (require && !this->has_enabled_broadcast_on_sock) {
    if (setsockopt(this->sock, SOL_SOCKET, SO_BROADCAST,
                   &enable, sizeof(enable)))
      return WSAGetLastError();

    this->has_enabled_broadcast_on_sock = 1;
  }

  return 0;
}

int praef_umb_set_use_broadcast(praef_message_bus* vthis,
                                int is_enabled) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  int error;

  if (0 == this->num_well_known_ports) {
#ifdef WSAEINVAL
    return WSAEINVAL;
#else
    return EINVAL;
#endif
  }

  if ((error = praef_umb_ensure_sock_broadcast(this, is_enabled)))
    return error;

  this->use_broadcast = is_enabled;
  return 0;
}

int praef_umb_set_spam_firewall(praef_message_bus* vthis,
                                int is_enabled) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  int error;

  if ((error = praef_umb_ensure_sock_broadcast(this, is_enabled)))
    return error;

  this->spam_firewall = is_enabled;
  return 0;
}

const PraefNetworkIdentifierPair_t*
praef_umb_local_address(praef_message_bus* vthis) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;

  return &this->local_netid;
}

const PraefNetworkIdentifierPair_t*
praef_umb_global_address(praef_message_bus* vthis) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;

  if (this->global_netid.internet)
    return &this->global_netid;
  else
    return NULL;
}

int praef_umb_lookup_vertex(praef_message_bus* vthis,
                            const char* hostname,
                            unsigned short port) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  struct addrinfo hint, * results;

  memset(&hint, 0, sizeof(hint));
  hint.ai_family = IPV(this->ip_version, PF_INET, PF_INET6);
  hint.ai_socktype = SOCK_DGRAM;

  if (getaddrinfo(hostname, NULL, &hint, &results))
    return WSAGetLastError();

  if (results->ai_addrlen > sizeof(combined_sockaddr)) {
    /* Unexpected. No code really fits here. */
    freeaddrinfo(results);
#ifdef WSA_NOT_ENOUGH_MEMORY
    return WSA_NOT_ENOUGH_MEMORY;
#else
    return ENOMEM;
#endif
  }

  memcpy(&this->vertex_address, results->ai_addr, results->ai_addrlen);
  this->has_vertex_address = 1;
  freeaddrinfo(results);
  return 0;
}

static void praef_umb_bcastaddr(combined_sockaddr* dst,
                                praef_umb_ip_version ipv) {
  memset(dst, 0, sizeof(combined_sockaddr));

  if (praef_uiv_ipv4 == ipv) {
    dst->ipv4.sin_family = AF_INET;
    dst->ipv4.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  } else {
    dst->ipv6.sin6_family = AF_INET6;
    /* All nodes on link */
    dst->ipv6.sin6_addr.s6_addr[ 0] = 0xFF;
    dst->ipv6.sin6_addr.s6_addr[ 1] = 0x02;
    dst->ipv6.sin6_addr.s6_addr[15] = 0x01;
  }
}

static int praef_umb_do_broadcast(praef_udp_message_bus* this,
                                  const void* data, size_t sz) {
  combined_sockaddr addr;
  unsigned i;

  praef_umb_bcastaddr(&addr, this->ip_version);
  for (i = 0; i < this->num_well_known_ports; ++i) {
    *IPV(this->ip_version, &addr.ipv4.sin_port, &addr.ipv6.sin6_port) =
      htons(this->well_known_ports[i]);

    if (SOCKET_ERROR ==
        sendto(this->sock, data, sz, 0,
               (struct sockaddr*)&addr, SASZ(this->ip_version)))
      return WSAGetLastError();
  }

  return 0;
}

int praef_umb_send_discovery(praef_message_bus* vthis) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  int error;

  if (INVALID_SOCKET == this->sock)
    return this->error;

  if (this->use_vertex) {
    if (!this->has_vertex_address) {
#ifdef WSAENOTCONN
      return WSAENOTCONN;
#else
      return ENOTCONN;
#endif
    }

    if (SOCKET_ERROR ==
        sendto(this->sock, this->discovery_packet,
               this->discovery_packet_size, 0,
               (struct sockaddr*)&this->vertex_address,
               SASZ(this->ip_version)))
      return WSAGetLastError();
  } else {
    if ((error = praef_umb_ensure_sock_broadcast(this, 1)))
      return error;

    if ((error = praef_umb_do_broadcast(
           this, this->discovery_packet,
           this->discovery_packet_size)))
      return error;
  }

  return 0;
}

int praef_umb_get_error(const praef_message_bus* vthis) {
  const praef_udp_message_bus* this = (const praef_udp_message_bus*)vthis;

  return this->error;
}

const char* praef_umb_get_error_context(const praef_message_bus* vthis) {
  const praef_udp_message_bus* this = (const praef_udp_message_bus*)vthis;

  return this->error_context;
}

#define RETNZ(n) do { int _n = !!(n); if (_n) return _n; } while (0)
#define C(a,b) RETNZ(((a) > (b)) - ((b) > (a)))
static int praef_compare_umb_network_identifier(
  const PraefNetworkIdentifier_t* a,
  const PraefNetworkIdentifier_t* b
) {
  C(a->port, b->port);
  C(a->address.present, b->address.present);
  if (PraefIpAddress_PR_ipv4 == a->address.present)
    RETNZ(memcmp(a->address.choice.ipv4.buf,
                 b->address.choice.ipv4.buf,
                 a->address.choice.ipv4.size));
  else
    RETNZ(memcmp(a->address.choice.ipv6.buf,
                 b->address.choice.ipv6.buf,
                 a->address.choice.ipv6.size));

  return 0;
}

static int praef_compare_umb_routes(const praef_umb_route* a,
                                    const praef_umb_route* b) {
  if (a->netid == b->netid) return 0;

  RETNZ(praef_compare_umb_network_identifier(&a->netid->intranet,
                                             &b->netid->intranet));
  C(!!a->netid->internet, !!b->netid->internet);
  if (a->netid->internet)
    RETNZ(praef_compare_umb_network_identifier(a->netid->internet,
                                               b->netid->internet));

  return 0;
}
#undef C
#undef RETNZ

static int praef_umb_create_route(praef_message_bus* vthis,
                                  const PraefNetworkIdentifierPair_t* netid) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  praef_umb_route example, * new_route;

  example.netid = netid;
  if (RB_FIND(praef_umb_route_tree, &this->routes, &example))
    /* Already present, nothing to do */
    return 1;

  new_route = malloc(sizeof(praef_umb_route));
  if (!new_route) return 0;

  new_route->netid = netid;
  RB_INSERT(praef_umb_route_tree, &this->routes, new_route);
  return 1;
}

static int praef_umb_delete_route(praef_message_bus* vthis,
                                  const PraefNetworkIdentifierPair_t* netid) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  praef_umb_route example, * existing;

  example.netid = netid;
  existing = RB_FIND(praef_umb_route_tree, &this->routes, &example);
  if (existing) {
    RB_REMOVE(praef_umb_route_tree, &this->routes, existing);
    free(existing);
    return 1;
  } else {
    return 0;
  }
}

static int praef_umb_on_same_network(
  const PraefNetworkIdentifierPair_t* a,
  const PraefNetworkIdentifierPair_t* b
) {
  if (!a->internet || !b->internet ||
      a->internet->address.present != b->internet->address.present)
    return 1;

  if (PraefIpAddress_PR_ipv4 == a->internet->address.present)
    return !memcmp(a->internet->address.choice.ipv4.buf,
                   b->internet->address.choice.ipv4.buf,
                   a->internet->address.choice.ipv4.size);
  else
    return !memcmp(a->internet->address.choice.ipv6.buf,
                   b->internet->address.choice.ipv6.buf,
                   a->internet->address.choice.ipv6.size);
}

static void praef_umb_single_netid_to_sockaddr(
  combined_sockaddr* dst, const PraefNetworkIdentifier_t* id
) {
  if (PraefIpAddress_PR_ipv4 == id->address.present) {
    dst->ipv4.sin_family = AF_INET;
    dst->ipv4.sin_port = htons(id->port);
    memcpy(&dst->ipv4.sin_addr.s_addr, id->address.choice.ipv4.buf,
           id->address.choice.ipv4.size);
  } else {
    dst->ipv6.sin6_family = AF_INET6;
    dst->ipv6.sin6_port = htons(id->port);
    memcpy(dst->ipv6.sin6_addr.s6_addr, id->address.choice.ipv6.buf,
           id->address.choice.ipv6.size);
  }
}

static void praef_umb_netid_to_sockaddr(
  combined_sockaddr* dst,
  const praef_udp_message_bus* this,
  const PraefNetworkIdentifierPair_t* netid
) {
  if (praef_umb_on_same_network(netid, &this->global_netid))
    praef_umb_single_netid_to_sockaddr(dst, &netid->intranet);
  else
    praef_umb_single_netid_to_sockaddr(dst, netid->internet);
}

static void praef_umb_unicast(praef_message_bus* vthis,
                              const PraefNetworkIdentifierPair_t* target,
                              const void* data, size_t sz) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  combined_sockaddr address;

  praef_umb_netid_to_sockaddr(&address, this, target);
  (void)sendto(this->sock, data, sz, 0,
               (struct sockaddr*)&address, SASZ(this->ip_version));
}

static void praef_umb_triangular_unicast(
  praef_message_bus* vthis,
  const PraefNetworkIdentifierPair_t* target,
  const void* data, size_t sz
) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  PraefUdpMsg_t trimsg;
  unsigned char tribuf[600];
  size_t tribuf_sz;

  praef_umb_unicast(vthis, target, data, sz);

  if (this->has_vertex_address && this->use_vertex && sz <= 511) {
    memset(&trimsg, 0, sizeof(trimsg));
    trimsg.present = PraefUdpMsg_PR_echo;
    trimsg.choice.echo.dst = *target;
    trimsg.choice.echo.data.buf = (void*)data;
    trimsg.choice.echo.data.size = sz;
    tribuf_sz = praef_umb_per_encode(tribuf, sizeof(tribuf), &trimsg,
                                     "Failed to encode ECHO message");
    (void)sendto(this->sock, tribuf, tribuf_sz, 0,
                 (struct sockaddr*)&this->vertex_address,
                 SASZ(this->ip_version));
  }
}

static void praef_umb_broadcast(
  praef_message_bus* vthis,
  const void* data, size_t sz
) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  praef_umb_route* route;

  if (this->use_broadcast) {
    (void)praef_umb_do_broadcast(this, data, sz);
  } else {
    RB_FOREACH(route, praef_umb_route_tree, &this->routes)
      praef_umb_unicast(vthis, route->netid, data, sz);
  }
}

static void praef_umb_maintain_vertex_cxn(praef_udp_message_bus*, time_t);
static void praef_umb_keep_firewall_open(praef_udp_message_bus*, time_t);
static void praef_umb_handle_internal_packet(praef_udp_message_bus*,
                                             const void*, size_t,
                                             const combined_sockaddr*);
static size_t praef_umb_recv(void* dst, size_t sz,
                             praef_message_bus* vthis) {
  praef_udp_message_bus* this = (praef_udp_message_bus*)vthis;
  ssize_t received;
  combined_sockaddr return_address;
  socklen_t return_address_sz = SASZ(this->ip_version);
  time_t now = time(NULL);

  praef_umb_maintain_vertex_cxn(this, now);
  praef_umb_keep_firewall_open(this, now);

  do {
    /* Assumption: dst is actually large enough for the packets used by this
     * implementation itself. This is always true for real praefectus
     * connections, since every message is prefixed with a DSA signature and
     * the minimum MTU is over 300 bytes anyway.
     */
    received = recvfrom(this->sock, dst, sz, 0,
                        (struct sockaddr*)&return_address,
                        &return_address_sz);

    if (SOCKET_ERROR == received) return 0; /* Socket buffer empty */
  } while (0 == received ||
           (size_t)received > sz); /* Skip empty and oversized packets */

  praef_umb_handle_internal_packet(this, dst, received, &return_address);

  return received;
}

static void praef_umb_maintain_vertex_cxn(praef_udp_message_bus* this,
                                          time_t now) {
  PraefUdpMsg_t msg;
  unsigned char buf[2048];
  size_t buf_sz;

  if (!this->use_vertex ||
      !this->has_vertex_address ||
      now == this->last_vertex_comm)
    return;

  memset(&msg, 0, sizeof(msg));
  if (!this->global_netid.internet) {
    msg.present = PraefUdpMsg_PR_whoami;
    msg.choice.whoami.local = this->global_netid.intranet;
  } else if (this->has_advert) {
    msg.present = PraefUdpMsg_PR_register;
    msg.choice.Register.fordiscovery = this->discovery_struct;
    msg.choice.Register.respondwith.sysid = this->advert.sysid;
    msg.choice.Register.respondwith.netid = this->global_netid;
    msg.choice.Register.respondwith.data.buf = this->advert.data;
    msg.choice.Register.respondwith.data.size = this->advert.data_size;
  } else {
    msg.present = PraefUdpMsg_PR_ping;
  }

  buf_sz = praef_umb_per_encode(buf, sizeof(buf), &msg,
                                "Failed to encode vertex message");
  (void)sendto(this->sock, buf, buf_sz, 0,
               (struct sockaddr*)&this->vertex_address, SASZ(this->ip_version));
  this->last_vertex_comm = now;
}

static void praef_umb_keep_firewall_open(praef_udp_message_bus* this,
                                         time_t now) {
  unsigned char ch = 255;

  if (!this->spam_firewall || now == this->last_firewall_spam)
    return;

  (void)praef_umb_do_broadcast(this, &ch, 1);
  this->last_firewall_spam = now;
}

static void praef_umb_handle_discover(praef_udp_message_bus*,
                                      const PraefUdpMsgDiscover_t*,
                                      const combined_sockaddr*);
static void praef_umb_handle_youare(praef_udp_message_bus*,
                                    const PraefUdpMsgYouAre_t*,
                                    const combined_sockaddr*);
static void praef_umb_handle_advert(praef_udp_message_bus*,
                                    const PraefUdpMsgAdvertise_t*);
static void praef_umb_handle_internal_packet(
  praef_udp_message_bus* this,
  const void* data, size_t sz,
  const combined_sockaddr* return_address
) {
  PraefUdpMsg_t deserialised, * deserialised_ptr = &deserialised;
  asn_dec_rval_t decode_result;

  memset(&deserialised, 0, sizeof(deserialised));
  decode_result = uper_decode_complete(
    NULL, &asn_DEF_PraefUdpMsg, (void**)&deserialised_ptr,
    data, sz);

  if (RC_OK != decode_result.code) return;

  if (sz == decode_result.consumed) {
    switch (deserialised.present) {
    case PraefUdpMsg_PR_NOTHING: /* Shouldn't happen, but treat as ping */
    case PraefUdpMsg_PR_ping: /* Nothing to do */
    case PraefUdpMsg_PR_whoami: /* Not intended for clients */
    case PraefUdpMsg_PR_register: /* Not intended for clients */
    case PraefUdpMsg_PR_echo: /* Not intended for clients */
      break;

    case PraefUdpMsg_PR_discover:
      praef_umb_handle_discover(this, &deserialised.choice.discover,
                                return_address);
      break;

    case PraefUdpMsg_PR_youare:
      praef_umb_handle_youare(this, &deserialised.choice.youare,
                              return_address);
      break;

    case PraefUdpMsg_PR_advertise:
      praef_umb_handle_advert(this, &deserialised.choice.advertise);
      break;
    }
  }

  (*asn_DEF_PraefUdpMsg.free_struct)(&asn_DEF_PraefUdpMsg, &deserialised, 1);
}

static void praef_umb_handle_discover(praef_udp_message_bus* this,
                                      const PraefUdpMsgDiscover_t* msg,
                                      const combined_sockaddr* return_address) {
  PraefUdpMsg_t response;
  unsigned char buf[2048];
  size_t sz;

  if (!this->listen_discover || /* Not servicing this message type */
      !this->has_advert || /* Can't service this message type */
      this->discovery_struct.application.size != msg->application.size ||
      memcmp(this->discovery_struct.application.buf, msg->application.buf,
             msg->application.size) || /* Different application */
      this->discovery_struct.version.size != msg->version.size ||
      memcmp(this->discovery_struct.version.buf, msg->version.buf,
             msg->version.size) /* Different version */)
    return;

  memset(&response, 0, sizeof(response));
  response.present = PraefUdpMsg_PR_advertise;
  response.choice.advertise.sysid = this->advert.sysid;
  response.choice.advertise.netid = this->local_netid;
  response.choice.advertise.data.buf = this->advert.data;
  response.choice.advertise.data.size = this->advert.data_size;

  sz = praef_umb_per_encode(buf, sizeof(buf), &response,
                            "Failed to encode ADVERTISE");
  (void)sendto(this->sock, buf, sz, 0, (struct sockaddr*)return_address,
               SASZ(this->ip_version));
}

static void praef_umb_handle_youare(praef_udp_message_bus* this,
                                    const PraefUdpMsgYouAre_t* msg,
                                    const combined_sockaddr* return_address) {
  /* Ignore if not from vertex */
  if (!this->has_vertex_address || !this->use_vertex) return;
  if (praef_uiv_ipv4 == this->ip_version) {
    if (this->vertex_address.ipv4.sin_port != return_address->ipv4.sin_port)
      return;
    if (memcmp(&this->vertex_address.ipv4.sin_addr.s_addr,
               &return_address->ipv4.sin_addr.s_addr, 4))
      return;
  } else {
    if (this->vertex_address.ipv6.sin6_port != return_address->ipv6.sin6_port)
      return;
    if (memcmp(this->vertex_address.ipv6.sin6_addr.s6_addr,
               return_address->ipv6.sin6_addr.s6_addr, 8*2))
      return;
  }

  /* Ignore if it doesn't have our own intranet netid */
  if (0 != praef_compare_umb_network_identifier(
        &this->global_netid.intranet, &msg->netid.intranet))
    return;

  /* Ignore if it lacks an internet id, or has an internet id of the incorrect
   * type.
   */
  if (!msg->netid.internet || msg->netid.internet->address.present !=
      msg->netid.intranet.address.present)
    return;

  /* Sane enough, update our global id */
  this->internet_id = *msg->netid.internet;
  this->global_netid.internet = &this->internet_id;
  if (PraefIpAddress_PR_ipv4 == this->internet_id.address.present) {
    memcpy(this->internet_ip, this->internet_id.address.choice.ipv4.buf,
           this->internet_id.address.choice.ipv4.size);
    this->internet_id.address.choice.ipv4.buf = this->internet_ip;
  } else {
    memcpy(this->internet_ip, this->internet_id.address.choice.ipv6.buf,
           this->internet_id.address.choice.ipv6.size);
    this->internet_id.address.choice.ipv6.buf = this->internet_ip;
  }
}

static void praef_umb_handle_advert(praef_udp_message_bus* this,
                                    const PraefUdpMsgAdvertise_t* msg) {
  PraefIpAddress_PR expected_ipv;
  praef_umb_advert advert;

  if (!this->listen_advert) return;

  /* Ensure it's of the correct IP version; otherwise discard */
  expected_ipv = this->local_netid.intranet.address.present;
  if (expected_ipv != msg->netid.intranet.address.present ||
      (msg->netid.internet &&
       expected_ipv != msg->netid.internet->address.present))
    return;

  /* Good enough, let the application know about it */
  advert.sysid = msg->sysid;
  memcpy(advert.data, msg->data.buf, msg->data.size);
  advert.data_size = msg->data.size;
  (*this->listen_advert)(this->listen_advert_userdata,
                         &advert, &msg->netid);
}
