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
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#else

#error "vertexd is only supported under Berkeley Sockets."

#endif

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <time.h>

#include "bsd.h"

#include "../udp-common/PraefUdpMsg.h"
#include "config/VertexdConfiguration.h"

typedef union {
  struct sockaddr_in ipv4;
  struct sockaddr_in6 ipv6;
} combined_sockaddr;

typedef struct {
  unsigned char s[2 + 2*8];
} normalised_sockaddr;

typedef time_t utime_t;
typedef struct pv_client_s {
  normalised_sockaddr address;
  time_t last_recv;
  utime_t last_echo, last_discover;

  char application[16+1];
  char version[8+1];
  unsigned char advert[1068];
  unsigned advert_size;
  unsigned sysid;
  int has_advert;

  TAILQ_ENTRY(pv_client_s) list;
  SPLAY_ENTRY(pv_client_s) tree;
} pv_client;

TAILQ_HEAD(pv_client_list,pv_client_s);
SPLAY_HEAD(pv_client_tree,pv_client_s);

typedef struct pv_realm_s {
  Realm_t config;
  PraefIpAddress_PR ipv;

  struct pv_client_list active_clients_lru;
  struct pv_client_list free_clients;
  struct pv_client_tree client_tree;

  pv_client clients[FLEXIBLE_ARRAY_MEMBER];
} pv_realm;

/* Indexed by fd */
static pv_realm* realms[FD_SETSIZE];

static int pv_compare_client(const pv_client*, const pv_client*);
static void print_help_and_exit(FILE*, int status, const char*);
static VertexdConfiguration_t* read_config(const char*);
static int create_realms(const VertexdConfiguration_t*);
static void run(unsigned maxfd);
static void update_realm(unsigned);
static void process_packet(unsigned, const normalised_sockaddr*,
                           const struct sockaddr*,
                           socklen_t,
                           const unsigned char*, size_t);
static void maintain_realm(pv_realm*);
static pv_client* get_client(pv_realm*, const normalised_sockaddr*,
                             const struct timespec*);
static void normalise_address(normalised_sockaddr* dst,
                              const combined_sockaddr* src);
static void sockaddr_from_normalised(combined_sockaddr* dst,
                                     const normalised_sockaddr* src,
                                     const pv_realm*);
static void handle_discover(unsigned, pv_client*,
                            const struct sockaddr*,
                            socklen_t,
                            const struct timespec*,
                            const PraefUdpMsgDiscover_t*);
static void handle_whoami(unsigned, pv_client*,
                          const struct sockaddr*,
                          socklen_t,
                          const struct timespec*,
                          const PraefUdpMsgWhoAmI_t*);
static void handle_register(unsigned, pv_client*,
                            const struct timespec*,
                            const PraefUdpMsgRegister_t*);
static void handle_echo(unsigned, pv_client*,
                        const struct timespec*,
                        const PraefUdpMsgEcho_t*);
static void handle_deregister(unsigned, pv_client*);
static utime_t to_micros(const struct timespec*);

SPLAY_PROTOTYPE(pv_client_tree, pv_client_s, tree, pv_compare_client)
SPLAY_GENERATE(pv_client_tree, pv_client_s, tree, pv_compare_client)

int main(signed /* required by clang */ argc, const char*const* argv) {
  VertexdConfiguration_t* config;
  int maxfd;

  if (2 != argc)
    print_help_and_exit(stderr, EX_USAGE, argv[0]);

  if (!strcmp("-?", argv[1]) ||
      !strcmp("-h", argv[1]) ||
      !strcmp("-help", argv[1]) ||
      !strcmp("--help", argv[1]))
    print_help_and_exit(stdout, 0, argv[0]);

  config = read_config(argv[1]);
  maxfd = create_realms(config);
  (*asn_DEF_VertexdConfiguration.free_struct)(
    &asn_DEF_VertexdConfiguration, config, 0);

  run(maxfd);

  return 0;
}

static int pv_compare_client(const pv_client* a, const pv_client* b) {
  return memcmp(&a->address.s, &b->address.s, sizeof(a->address.s));
}

static void print_help_and_exit(FILE* out, int status, const char* progname) {
  fprintf(out, "Usage: %s <configfile>\n", progname);
  exit(status);
}

static VertexdConfiguration_t* read_config(const char* filename) {
  asn_codec_ctx_t ctx;
  VertexdConfiguration_t* config = NULL;
  asn_dec_rval_t result;
  FILE* in;
  char data[4096];
  size_t nread, data_size;
  unsigned off = 0;

  in = fopen(filename, "rb");
  if (!in)
    err(EX_NOINPUT, "open(%s)", filename);

  memset(&ctx, 0, sizeof(ctx));

  do {
    nread = fread(data+off, 1, sizeof(data)-off, in);
    if (0 == nread) {
      if (feof(in))
        err(EX_CONFIG, "Unexpected EOF reading input");
      else
        err(EX_IOERR, "I/O error reading input");
    }

    data_size = nread + off;
    result = xer_decode(&ctx, &asn_DEF_VertexdConfiguration,
                        (void**)&config,
                        data, data_size);

    if (RC_WMORE == result.code) {
      off = data_size - result.consumed;
      memmove(data, data + result.consumed, off);

      if (off >= sizeof(data))
        errx(EX_CONFIG, "Out of buffer space reading input");
    }
  } while (RC_WMORE == result.code);

  /* We should probably give better error messages here.
   *
   * The config is small enough that it shouldn't be too much of an issue,
   * though.
   */

  if (RC_OK != result.code)
    errx(EX_CONFIG, "Error in configuration");

  if ((*asn_DEF_VertexdConfiguration.check_constraints)(
        &asn_DEF_VertexdConfiguration, config, NULL, NULL))
    errx(EX_CONFIG, "Constraint violation in configuration");

  fclose(in);

  return config;
}

static int create_realms(const VertexdConfiguration_t* config) {
  int sock, maxsock = 0;
  unsigned i, j;
  struct addrinfo hints, * addrinfo;
  const Realm_t* realm_config;
  int error;
  char servname[6];

  for (i = 0; i < (unsigned)config->realms.list.count; ++i) {
    realm_config = config->realms.list.array[i];

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags |= AI_NUMERICHOST;
    hints.ai_flags |= AI_PASSIVE;
    snprintf(servname, sizeof(servname), "%d", (int)realm_config->port);
    if ((error = getaddrinfo((const char*)realm_config->localaddr.buf,
                             servname, &hints, &addrinfo)))
      errx(EX_NOHOST, "getaddrinfo(%s): %s", realm_config->localaddr.buf,
           gai_strerror(error));

    if (PF_INET != addrinfo->ai_family && PF_INET6 != addrinfo->ai_family)
      errx(EX_NOHOST, "not an IPv4 or IPv6 numeric address: %s",
           realm_config->localaddr.buf);

    sock = socket(addrinfo->ai_family, SOCK_DGRAM, IPPROTO_UDP);
    if (-1 == sock)
      err(EX_OSERR, "creating socket for %s", realm_config->localaddr.buf);
    if ((unsigned)sock >= FD_SETSIZE)
      errx(EX_UNAVAILABLE, "socket number is greater than FD_SETSIZE (%d): "
           "%d, for realm: %s", FD_SETSIZE, sock, realm_config->localaddr.buf);

    if (sock > maxsock) maxsock = sock;

    if (bind(sock, addrinfo->ai_addr, addrinfo->ai_addrlen))
      err(EX_OSERR, "binding socket to %s", realm_config->localaddr.buf);

    realms[sock] = calloc(1, offsetof(pv_realm, clients) +
                          sizeof(pv_client) * realm_config->maxclients);
    if (!realms[sock])
      errx(EX_UNAVAILABLE, "out of memory creating realm %s",
           realm_config->localaddr.buf);

    realms[sock]->config = *realm_config;
    /* Null out the string so we can't accidentally use freed memory later */
    realms[sock]->config.localaddr.buf = NULL;
    realms[sock]->ipv = (PF_INET == addrinfo->ai_family?
                         PraefIpAddress_PR_ipv4 :
                         PraefIpAddress_PR_ipv6);
    TAILQ_INIT(&realms[sock]->active_clients_lru);
    TAILQ_INIT(&realms[sock]->free_clients);
    SPLAY_INIT(&realms[sock]->client_tree);
    for (j = realm_config->maxclients-1;
         j < (unsigned)realm_config->maxclients; --j) {
      TAILQ_INSERT_HEAD(&realms[sock]->free_clients,
                        realms[sock]->clients+j, list);
    }

    freeaddrinfo(addrinfo);
  }

  return maxsock;
}

static void run(unsigned maxfd) {
  fd_set readfds, imm_readfds;
  unsigned i;
  int selected;

  FD_ZERO(&imm_readfds);
  for (i = 0; i <= maxfd; ++i)
    if (realms[i])
      FD_SET(i, &imm_readfds);

  for (;;) {
    readfds = imm_readfds;
    selected = select(maxfd+1, &readfds, NULL, NULL, NULL);

    if (0 == selected)
      errx(EX_SOFTWARE, "select() returned unexpectedly");

    if (-1 == selected && EINTR != errno)
      err(EX_OSERR, "select()");

    for (i = 0; i <= maxfd; ++i)
      if (realms[i] && FD_ISSET(i, &readfds))
        update_realm(i);
  }
}

static void update_realm(unsigned sock) {
  pv_realm* realm = realms[sock];
  combined_sockaddr from_addr;
  socklen_t from_addr_len = sizeof(from_addr);
  normalised_sockaddr norm_from_addr;
  unsigned char buf[2048];
  ssize_t packet_size;

  packet_size = recvfrom(sock, buf, sizeof(buf), 0,
                         (struct sockaddr*)&from_addr, &from_addr_len);
  normalise_address(&norm_from_addr, &from_addr);
  if (-1 != packet_size)
    process_packet(sock, &norm_from_addr,
                   (const struct sockaddr*)&from_addr, from_addr_len,
                   buf, packet_size);

  maintain_realm(realm);
}

static void process_packet(unsigned sock,
                           const normalised_sockaddr* from_addr,
                           const struct sockaddr* return_addr,
                           socklen_t return_addr_sz,
                           const unsigned char* data, size_t sz) {
  pv_realm* realm = realms[sock];
  pv_client* client;
  PraefUdpMsg_t decoded, * decoded_ptr = &decoded;
  asn_dec_rval_t decode_result;
  struct timespec now;

  /* See whether the message is valid; if not, just drop the whole thing. */
  memset(&decoded, 0, sizeof(decoded));
  decode_result = uper_decode_complete(
    NULL, &asn_DEF_PraefUdpMsg, (void**)&decoded_ptr,
    data, sz);
  if (RC_OK != decode_result.code) goto end;
  if ((*asn_DEF_PraefUdpMsg.check_constraints)(
        &asn_DEF_PraefUdpMsg, &decoded, NULL, NULL))
    goto end;

  if (clock_gettime(CLOCK_REALTIME, &now))
    err(EX_OSERR, "clock_gettime");

  client = get_client(realm, from_addr, &now);
  if (!client)
    /* At capacity */
    goto end;

  switch (decoded.present) {
  case PraefUdpMsg_PR_discover:
    handle_discover(sock, client, return_addr, return_addr_sz, &now,
                    &decoded.choice.discover);
    break;

  case PraefUdpMsg_PR_whoami:
    handle_whoami(sock, client, return_addr, return_addr_sz, &now,
                  &decoded.choice.whoami);
    break;

  case PraefUdpMsg_PR_register:
    handle_register(sock, client, &now, &decoded.choice.Register);
    break;

  case PraefUdpMsg_PR_echo:
    handle_echo(sock, client, &now, &decoded.choice.echo);
    break;

  default:
    /* Anything else (just Ping from a correct client) indicates the client has
     * nothing to advertise, and thus is to be deregistered (but still
     * maintained as a client).
     */
    handle_deregister(sock, client);
    break;
  }

  end:
  (*asn_DEF_PraefUdpMsg.free_struct)(&asn_DEF_PraefUdpMsg, &decoded, 1);
}

static void normalise_address(normalised_sockaddr* dst,
                              const combined_sockaddr* src) {
  if (AF_INET == src->ipv4.sin_family) {
    dst->s[0] = (src->ipv4.sin_port >> 0) & 0xFF;
    dst->s[1] = (src->ipv4.sin_port >> 8) & 0xFF;
    memcpy(dst->s+2, &src->ipv4.sin_addr.s_addr, 4);
    memset(dst->s+6, 0, 12);
  } else {
    dst->s[0] = (src->ipv6.sin6_port >> 0) & 0xFF;
    dst->s[1] = (src->ipv6.sin6_port >> 8) & 0xFF;
    memcpy(dst->s+2, src->ipv6.sin6_addr.s6_addr, 16);
  }
}

static void sockaddr_from_normalised(combined_sockaddr* dst,
                                     const normalised_sockaddr* src,
                                     const pv_realm* realm) {
  if (PraefIpAddress_PR_ipv4 == realm->ipv) {
    dst->ipv4.sin_family = AF_INET;
    dst->ipv4.sin_port = src->s[0] | (src->s[1] << 8);
    memcpy(&dst->ipv4.sin_addr.s_addr, src->s+2, 4);
  } else {
    dst->ipv6.sin6_family = AF_INET6;
    dst->ipv6.sin6_port = src->s[0] | (src->s[1] << 8);
    memcpy(dst->ipv6.sin6_addr.s6_addr, src->s+2, 16);
  }
}

static pv_client* get_client(pv_realm* realm, const normalised_sockaddr* addr,
                             const struct timespec* now) {
  pv_client example, * client;

  example.address = *addr;
  client = SPLAY_FIND(pv_client_tree, &realm->client_tree, &example);
  if (client) {
    /* Already exists. Move to head of LRU queue and ping its timestamp, but
     * otherwise there's nothing special to do.
     */
    TAILQ_REMOVE(&realm->active_clients_lru, client, list);
  } else {
    /* No current mapping. Try to get an unused one */
    client = TAILQ_FIRST(&realm->free_clients);
    if (!client) /* At capacity */ return NULL;

    /* OK, initialise new client */
    TAILQ_REMOVE(&realm->free_clients, client, list);
    memset(client, 0, sizeof(pv_client));
    client->address = *addr;
    SPLAY_INSERT(pv_client_tree, &realm->client_tree, client);
  }

  TAILQ_INSERT_HEAD(&realm->active_clients_lru, client, list);
  client->last_recv = now->tv_sec;
  return client;
}

static utime_t to_micros(const struct timespec* ts) {
  utime_t sec = ts->tv_sec, usec = ts->tv_nsec/1000;
  return sec*1000000 + usec;
}

typedef struct enumerated_sysid_s {
  unsigned sysid;
  RB_ENTRY(enumerated_sysid_s) tree;
} enumerated_sysid;
RB_HEAD(enumerated_sysid_tree, enumerated_sysid_s);

static int compare_enumerate_sysid(const enumerated_sysid* a,
                                   const enumerated_sysid* b) {
  return (a->sysid > b->sysid) - (b->sysid > a->sysid);
}

RB_PROTOTYPE_STATIC(enumerated_sysid_tree, enumerated_sysid_s,
                    tree, compare_enumerate_sysid)
RB_GENERATE_STATIC(enumerated_sysid_tree, enumerated_sysid_s,
                   tree, compare_enumerate_sysid)

static void handle_discover(unsigned sock, pv_client* client,
                            const struct sockaddr* reply_addr,
                            socklen_t reply_addr_sz,
                            const struct timespec* tsnow,
                            const PraefUdpMsgDiscover_t* msg) {
  pv_realm* realm = realms[sock];
  enumerated_sysid enumerated_array[realm->config.maxresponses], example;
  struct enumerated_sysid_tree enumerated = RB_INITIALIZER(&enumerated);
  unsigned num_enumerated = 0;
  utime_t now = to_micros(tsnow);
  pv_client* other;

  /* Discard if insufficient time has passed since last query */
  if (client->last_discover >= now - (utime_t)realm->config.discoverintervalus)
    return;

  /* OK, look for compatible clients with advertisements, until either all
   * clients have been examined or the distinct system limit has been reached.
   */
  TAILQ_FOREACH(other, &realm->active_clients_lru, list) {
    if (num_enumerated >= (unsigned)realm->config.maxresponses)
      break;

    if (!other->has_advert) continue; /* nothing to report */

    if (strncmp((const char*)msg->application.buf, other->application,
                 sizeof(other->application)) ||
        strncmp((const char*)msg->version.buf, other->version,
                 sizeof(other->version)))
      /* Incompatible client */
      continue;

    example.sysid = other->sysid;
    if (RB_FIND(enumerated_sysid_tree, &enumerated, &example))
      /* Already reported this system */
      continue;

    /* OK, as-yet unreported system */
    enumerated_array[num_enumerated].sysid = other->sysid;
    RB_INSERT(enumerated_sysid_tree, &enumerated,
              enumerated_array + num_enumerated);
    ++num_enumerated;

    if (sendto(sock, other->advert, other->advert_size, 0,
               reply_addr, reply_addr_sz) <= 0)
      warn("sendto(Advertise)");
  }

  client->last_discover = now;
}

static void handle_whoami(unsigned sock, pv_client* client,
                          const struct sockaddr* reply_addr,
                          socklen_t reply_addr_sz,
                          const struct timespec* tsnow,
                          const PraefUdpMsgWhoAmI_t* msg) {
  pv_realm* realm = realms[sock];
  PraefUdpMsg_t response;
  PraefNetworkIdentifier_t global_id;
  unsigned char global_ip_data[16], response_buf[256];
  asn_enc_rval_t encode_result;

  /* Check sanity of local address */
  if (realm->ipv != msg->local.address.present)
    return;

  /* Good enough */
  memset(&response, 0, sizeof(response));
  memset(&global_id, 0, sizeof(global_id));
  response.present = PraefUdpMsg_PR_youare;
  response.choice.youare.netid.intranet = msg->local;
  response.choice.youare.netid.internet = &global_id;
  if (PraefIpAddress_PR_ipv4 == realm->ipv) {
    response.choice.youare.netid.internet->port = (unsigned)ntohs(
      ((const struct sockaddr_in*)reply_addr)->sin_port);
    memcpy(global_ip_data,
           &((const struct sockaddr_in*)reply_addr)->sin_addr.s_addr, 4);
    response.choice.youare.netid.internet->address.present = realm->ipv;
    response.choice.youare.netid.internet->address.choice.ipv4.buf =
      global_ip_data;
    response.choice.youare.netid.internet->address.choice.ipv4.size = 4;
  } else {
    response.choice.youare.netid.internet->port = (unsigned)ntohs(
      ((const struct sockaddr_in6*)reply_addr)->sin6_port);
    memcpy(global_ip_data,
           ((const struct sockaddr_in6*)reply_addr)->sin6_addr.s6_addr, 16);
    response.choice.youare.netid.internet->address.present = realm->ipv;
    response.choice.youare.netid.internet->address.choice.ipv6.buf =
      global_ip_data;
    response.choice.youare.netid.internet->address.choice.ipv6.size = 16;
  }

  encode_result = uper_encode_to_buffer(
    &asn_DEF_PraefUdpMsg, &response, response_buf, sizeof(response_buf));
  if (-1 == encode_result.encoded) {
    warnx("failed to encode YouAre response");
    return;
  }

  if (sendto(sock, response_buf, (encode_result.encoded + 7) / 8, 0,
             reply_addr, reply_addr_sz) <= 0)
    warn("sendto(YouAre)");
}

static void handle_register(unsigned sock, pv_client* client,
                            const struct timespec* tsnow,
                            const PraefUdpMsgRegister_t* msg) {
  pv_realm* realm = realms[sock];
  PraefUdpMsg_t env_advert;
  asn_enc_rval_t encode_result;

  /* Ensure that the IP versions on the advertisement are appropriate, that the
   * identifier is global, and that the global address matches what we see.
   */
  if (realm->ipv != msg->respondwith.netid.intranet.address.present ||
      !msg->respondwith.netid.internet ||
      realm->ipv != msg->respondwith.netid.internet->address.present ||
      ntohs(client->address.s[0] | (client->address.s[1] << 8)) !=
          msg->respondwith.netid.internet->port ||
      memcmp(client->address.s+2,
             msg->respondwith.netid.internet->address.choice.ipv4.buf,
             msg->respondwith.netid.internet->address.choice.ipv4.size))
    return;

  /* OK, encode the result into the stored buffer, copy other parms */
  memset(&env_advert, 0, sizeof(env_advert));
  env_advert.present = PraefUdpMsg_PR_advertise;
  env_advert.choice.advertise = msg->respondwith;

  /* Clear in case we need to back out half way through */
  client->has_advert = 0;
  encode_result = uper_encode_to_buffer(
    &asn_DEF_PraefUdpMsg, &env_advert,
    client->advert, sizeof(client->advert));
  if (-1 == encode_result.encoded) {
    warnx("failed to store encoded Advertisement");
    return;
  }
  client->advert_size = (encode_result.encoded + 7) / 8;

  client->sysid = msg->respondwith.sysid;
  strlcpy(client->application, (const char*)msg->fordiscovery.application.buf,
          sizeof(client->application));
  strlcpy(client->version, (const char*)msg->fordiscovery.version.buf,
          sizeof(client->version));
  /* OK, this can be published */
  client->has_advert = 1;
}

static void handle_echo(unsigned sock, pv_client* client,
                        const struct timespec* tsnow,
                        const PraefUdpMsgEcho_t* msg) {
  pv_realm* realm = realms[sock];
  utime_t now = to_micros(tsnow);
  pv_client dst_example, * dst;
  unsigned porth;
  combined_sockaddr dstaddr;

  /* Drop if too large */
  if (msg->data.size > realm->config.maxechosize)
    return;

  /* Drop if last echo was too recent */
  if (client->last_echo >= now - (utime_t)realm->config.echointervalus)
    return;

  /* The destination must be sane */
  if (!msg->dst.internet ||
      realm->ipv != msg->dst.internet->address.present)
    return;

  /* The destination must exist and must not be the client */
  memset(&dst_example.address, 0, sizeof(normalised_sockaddr));
  porth = htons(msg->dst.internet->port);
  dst_example.address.s[0] = (porth >> 0) & 0xFF;
  dst_example.address.s[1] = (porth >> 8) & 0xFF;
  memcpy(dst_example.address.s+2,
         msg->dst.internet->address.choice.ipv4.buf,
         msg->dst.internet->address.choice.ipv4.size);
  dst = SPLAY_FIND(pv_client_tree, &realm->client_tree, &dst_example);
  if (!dst || client == dst) return;

  sockaddr_from_normalised(&dstaddr, &dst->address, realm);
  if (sendto(sock, msg->data.buf, msg->data.size, 0,
             (const struct sockaddr*)&dstaddr,
             PraefIpAddress_PR_ipv4 == realm->ipv?
             sizeof(struct sockaddr_in) :
             sizeof(struct sockaddr_in6)) <= 0)
    warnx("sendto(Echo)");

  client->last_echo = now;
}

static void handle_deregister(unsigned sock, pv_client* client) {
  client->has_advert = 0;
}

static void maintain_realm(pv_realm* realm) {
  pv_client* dead;
  struct timespec now;
  time_t clients_expire_before;

  if (clock_gettime(CLOCK_REALTIME, &now))
    err(EX_OSERR, "clock_gettime");

  clients_expire_before = now.tv_sec - realm->config.clientlifetimesecs;

  while (!TAILQ_EMPTY(&realm->active_clients_lru) &&
         clients_expire_before > TAILQ_LAST(&realm->active_clients_lru,
                                            pv_client_list)->last_recv) {
    dead = TAILQ_LAST(&realm->active_clients_lru, pv_client_list);
    TAILQ_REMOVE(&realm->active_clients_lru, dead, list);
    SPLAY_REMOVE(pv_client_tree, &realm->client_tree, dead);
    TAILQ_INSERT_HEAD(&realm->free_clients, dead, list);
  }
}
