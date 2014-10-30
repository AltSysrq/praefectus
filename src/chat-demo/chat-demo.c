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

/* We're just using this for SDL_Delay() since the rest of the project depends
 * on SDL anyway.
 */
#include <SDL.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <libpraefectus/system.h>
#include <libpraefectus/stdsys.h>
#include <libpraefectus/udp-message-bus/udp-message-bus.h>

#include "bsd.h"

/**
 * @file
 *
 * This file demonstrates using the UDP message bus with libpraefectus to
 * implement a simple console-based chat room. (Not that this is a good use
 * case for libpraefectus, but it serves the purpose well enough and is very
 * easy to test.) It assumes UNIX-like file control and terminals; the user
 * interface will not work correctly on Windows, except under Cygwin and such.
 * Error messages may not meaningful on Windows with Winsock.
 *
 * Each line (max 80 characters) read from stdin becomes a message displayed on
 * the console, and is represented as a single praefectus event. Messages
 * events are rolled back by sending the terminal control sequences to move the
 * cursor up one line, move back to the beginning of the line, and clear the
 * line. This does not play nicely with text entry, and strongly assumes an
 * xterm-like terminal. You may want to redirect stdout to another terminal so
 * that text input is unhindered.
 *
 * The demo supports using the full range of features offered by the UDP
 * message bus. However, it does not check whether its configured options are
 * sane; invalid command-line parameters can cause the program to abort.
 *
 * In terms of system discovery, each chat room is associated with a
 * human-readable name, stored in the advertisement appdata. System ids are
 * chosen based on the lower 8 bits of the current time; system ids above 255
 * are ignored. This is obviously far from scallable, but it simplifies the
 * demo.
 *
 * Diagnostic output is printed to stderr, including log messages about what
 * the system is trying to do.
 *
 * The process runs until killed or it encounters EOF on stdin. Stdin itself is
 * read in non-blocking mode; the process is a simple try-read-stdin, update
 * system, sleep for 100ms loop.
 */

#define STRMAX 80

typedef struct message_instant_s {
  praef_instant instant;

  SLIST_ENTRY(message_instant_s) next;
} message_instant;

typedef struct chat_object_s {
  praef_object self;

  SLIST_HEAD(,message_instant_s) messages;
  SLIST_ENTRY(chat_object_s) next;
} chat_object;


typedef struct {
  praef_event self;

  char str[STRMAX+1];
} chat_event;

static struct {
  char name[STRMAX+1];
  PraefNetworkIdentifierPair_t netid;
  PraefNetworkIdentifier_t netid_internet;
  unsigned char netid_local_addr[16], netid_global_addr[16];
} discoveries[256];

static praef_message_bus* bus;
static praef_system* sys;
static praef_app* app;
static praef_std_state app_state;
static const PraefNetworkIdentifierPair_t* netid;
static SLIST_HEAD(,chat_object_s) objects = SLIST_HEAD_INITIALIZER(objects);

static void chat_object_step(chat_object*, praef_userdata);
static void chat_object_rewind(chat_object*, praef_instant);
static void chat_event_apply(chat_object*, const chat_event*,
                             praef_userdata);
static void app_create_node_object(praef_app*, praef_object_id);
static praef_event* app_decode_event(praef_app*, praef_instant,
                                     praef_object_id,
                                     praef_event_serial_number,
                                     const void*, size_t);
static void app_acquire_id(praef_app*, praef_object_id);
static void app_discover_node(
  praef_app*, const PraefNetworkIdentifierPair_t*, praef_object_id);
static void app_remove_node(praef_app*, praef_object_id);
static void app_join_tree_traversed(praef_app*);
static void app_ht_scan_progress(praef_app*, unsigned, unsigned);
static void app_awaiting_stability(praef_app*, praef_object_id,
                                   praef_instant, praef_instant,
                                   praef_instant);
static void app_information_complete(praef_app*);
static void app_clock_synced(praef_app*);
static void app_gained_grant(praef_app*);
static void app_log(praef_app*, const char*);
static void receive_advert(praef_userdata, const praef_umb_advert*,
                           const PraefNetworkIdentifierPair_t*);
static void oom_if_not(int);
static void oom_if_notp(const void*);
static void print_netid(const PraefNetworkIdentifierPair_t*);
static void print_single_netid(const PraefNetworkIdentifier_t*);

static void configure_advert(unsigned, const char*);
static void connect_bootstrap(const char*);
static void connect_discover(const char*);
static void enable_broadcast(void);
static void set_vertex(const char*, const char*);
static void enable_local_listen(void);
static void enable_firewall_spam(void);

static void pump_network(void);
static void delay(void);
static void run(void);

static const unsigned short well_known_port = 29708;

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmain"
#endif
int main(unsigned argc, const char*const* argv) {
  unsigned ipv, i;
  chat_object* obj, * objtmp;
  message_instant* m, * mtmp;

  if (argc < 4)
    errx(EX_USAGE,
         "Usage: %s <ipv> [parameters] <connector>\n"
         "\n"
         "  <ipv> is either 4 or 6, indicating which version of \n"
         "  Internet Protocol to use.\n"
         "\n"
         "  <connector> can be any one of the following:\n"
         "    bootstrap <name>\n"
         "      Don't perform discovery; create a new system with\n"
         "      name <name>.\n"
         "    discover <name>\n"
         "      Perform discovery, waiting until a system with the\n"
         "      name can be found.\n"
         "\n"
         "  Options:\n"
         "    broadcast\n"
         "      Enable network-level broadcasting. This is required\n"
         "      for non-vertex discovery.\n"
         "    vertex <hostname> <port>\n"
         "      Use the vertex server on the given hostname and port\n"
         "      number.\n"
         "    listen\n"
         "      Listen for discovery requests on the local link.\n"
         "    firewall-spam\n"
         "      Enable firewall spamming.\n"
         "", argv[0]);

  ipv = atoi(argv[1]);
  if (4 != ipv && 6 != ipv)
    errx(EX_USAGE,
         "Invalid IP version: %s", argv[1]);

  if (praef_umb_application_init())
    errx(EX_OSERR, "Failed to initialise the networking subsystem.");

  oom_if_notp(bus = praef_umb_new("praef-chat-demo", "?",
                                  &well_known_port, 1,
                                  4 == ipv? praef_uiv_ipv4 :
                                            praef_uiv_ipv6));
  if (praef_umb_get_error(bus))
    errx(EX_OSERR, "Failed to create network bus: %s: %s",
         praef_umb_get_error_context(bus),
         strerror(praef_umb_get_error(bus)));

  netid = praef_umb_local_address(bus);
  fprintf(stderr, "Local address is ");
  print_single_netid(&netid->intranet);
  fprintf(stderr, "\n");

  for (i = 2; i < argc-2; ++i) {
    if (!strcmp("broadcast", argv[i]))
      enable_broadcast();
    else if (!strcmp("vertex", argv[i]))
      set_vertex(argv[i+1], argv[i+2]), i += 2;
    else if (!strcmp("listen", argv[i]))
      enable_local_listen();
    else if (!strcmp("firewall-spam", argv[i]))
      enable_firewall_spam();
    else
      errx(EX_USAGE, "Invalid option: %s", argv[i]);
  }

  oom_if_not(praef_std_state_init(&app_state));
  oom_if_notp(app = praef_stdsys_new(&app_state));
  oom_if_notp(sys = praef_system_new(
                app, bus, netid, 10, praef_sp_lax,
                4 == ipv? praef_siv_4only :
                          praef_siv_6only,
                netid->internet? praef_snl_global :
                                 praef_snl_local,
                512));
  praef_stdsys_set_system(app, sys);

  app->create_node_object = app_create_node_object;
  app->decode_event = app_decode_event;
  app->acquire_id_opt = app_acquire_id;
  app->discover_node_opt = app_discover_node;
  app->remove_node_opt = app_remove_node;
  app->join_tree_traversed_opt = app_join_tree_traversed;
  app->ht_scan_progress_opt = app_ht_scan_progress;
  app->awaiting_stability_opt = app_awaiting_stability;
  app->information_complete_opt = app_information_complete;
  app->clock_synced_opt = app_clock_synced;
  app->gained_grant_opt = app_gained_grant;
  app->log_opt = app_log;

  /* Reduce background network traffic */
  praef_system_conf_commit_interval(sys, 100);
  praef_system_conf_max_commit_lag(sys, 1000);
  praef_system_conf_max_validated_lag(sys, 2000);
  praef_system_conf_ht_root_query_interval(sys, 300);

  if (!strcmp("bootstrap", argv[argc-2]))
    connect_bootstrap(argv[argc-1]);
  else if (!strcmp("discover", argv[argc-2]))
    connect_discover(argv[argc-1]);
  else
    errx(EX_USAGE, "Invalid connector: %s", argv[argc-2]);

  run();

  praef_system_delete(sys);
  praef_stdsys_delete(app);
  praef_std_state_cleanup(&app_state);
  praef_umb_delete(bus);

  SLIST_FOREACH_SAFE(obj, &objects, next, objtmp) {
    SLIST_FOREACH_SAFE(m, &obj->messages, next, mtmp)
      free(m);
    free(obj);
  }

  return 0;
}

static void oom_if_not(int i) {
  if (!i)
    errx(EX_UNAVAILABLE, "Out of memory");
}

static void oom_if_notp(const void* p) {
  oom_if_not(!!p);
}

static void print_netid(const PraefNetworkIdentifierPair_t* id) {
  if (id->internet) {
    print_single_netid(id->internet);
    fprintf(stderr, "/");
  }
  print_single_netid(&id->intranet);
}

static void print_single_netid(const PraefNetworkIdentifier_t* id) {
  if (PraefIpAddress_PR_ipv4 == id->address.present)
    fprintf(stderr, "%d.%d.%d.%d",
            (unsigned)id->address.choice.ipv4.buf[0],
            (unsigned)id->address.choice.ipv4.buf[1],
            (unsigned)id->address.choice.ipv4.buf[2],
            (unsigned)id->address.choice.ipv4.buf[3]);
  else
    fprintf(stderr,
            "[%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
            "%02x%02x:%02x%02x:%02x%02x:%02x%02x]",
            (unsigned)id->address.choice.ipv6.buf[0],
            (unsigned)id->address.choice.ipv6.buf[1],
            (unsigned)id->address.choice.ipv6.buf[2],
            (unsigned)id->address.choice.ipv6.buf[3],
            (unsigned)id->address.choice.ipv6.buf[4],
            (unsigned)id->address.choice.ipv6.buf[5],
            (unsigned)id->address.choice.ipv6.buf[6],
            (unsigned)id->address.choice.ipv6.buf[7],
            (unsigned)id->address.choice.ipv6.buf[8],
            (unsigned)id->address.choice.ipv6.buf[9],
            (unsigned)id->address.choice.ipv6.buf[10],
            (unsigned)id->address.choice.ipv6.buf[11],
            (unsigned)id->address.choice.ipv6.buf[12],
            (unsigned)id->address.choice.ipv6.buf[13],
            (unsigned)id->address.choice.ipv6.buf[14],
            (unsigned)id->address.choice.ipv6.buf[15]);

  fprintf(stderr, ":%d", (unsigned)id->port);
}

static void chat_object_step(chat_object* this, praef_userdata _) {
}

static void chat_object_rewind(chat_object* this, praef_instant to) {
  message_instant* m;
  while (!SLIST_EMPTY(&this->messages) &&
         to <= SLIST_FIRST(&this->messages)->instant) {
    m = SLIST_FIRST(&this->messages);
    SLIST_REMOVE_HEAD(&this->messages, next);
    free(m);

    printf("\e[F\r\e[K");
  }
}

static void chat_event_apply(chat_object* target, const chat_event* evt,
                             praef_userdata _) {
  message_instant* m;

  printf("%02d:%02d:%02d.%d [%08X] %s\n",
         evt->self.instant / 10 / 60 / 60,
         evt->self.instant / 10 / 60 % 60,
         evt->self.instant / 10 % 60,
         evt->self.instant % 10,
         target->self.id, evt->str);

  oom_if_notp(m = malloc(sizeof(message_instant)));
  m->instant = evt->self.instant;
  SLIST_INSERT_HEAD(&target->messages, m, next);
}

static void app_create_node_object(praef_app* app, praef_object_id id) {
  chat_object* obj = malloc(sizeof(chat_object));

  oom_if_notp(obj);
  obj->self.step = (praef_object_step_t)chat_object_step;
  obj->self.rewind = (praef_object_rewind_t)chat_object_rewind;
  obj->self.id = id;
  SLIST_INIT(&obj->messages);
  SLIST_INSERT_HEAD(&objects, obj, next);

  praef_context_add_object(app_state.context, (praef_object*)obj);
}

static praef_event* app_decode_event(praef_app* app, praef_instant instant,
                                     praef_object_id object,
                                     praef_event_serial_number serno,
                                     const void* data,
                                     size_t sz) {
  chat_event* evt;

  if (sz > STRMAX) return NULL;

  evt = malloc(sizeof(chat_event));
  if (!evt) {
    praef_system_oom(sys);
    return NULL;
  }

  evt->self.apply = (praef_event_apply_t)chat_event_apply;
  evt->self.free = free;
  evt->self.instant = instant;
  evt->self.object = object;
  evt->self.serial_number = serno;
  memcpy(evt->str, data, sz);
  evt->str[sz] = 0;
  return (praef_event*)evt;
}

static void app_acquire_id(praef_app* _, praef_object_id id) {
  fprintf(stderr, "Acquired id: %08X\n", id);
}

static void app_discover_node(praef_app* _,
                              const PraefNetworkIdentifierPair_t* netid,
                              praef_object_id id) {
  fprintf(stderr, "Discovered node: %08X at ", id);
  print_netid(netid);
  fprintf(stderr, "\n");
}

static void app_remove_node(praef_app* _, praef_object_id id) {
  fprintf(stderr, "Removed node: %08X\n", id);
}

static void app_join_tree_traversed(praef_app* _) {
  fprintf(stderr, "Join tree traversal completed.\n");
}

static void app_ht_scan_progress(praef_app* _, unsigned num, unsigned denom) {
  static unsigned last_num = 0, last_denom = 0;

  if (num != last_num || denom != last_denom)
    fprintf(stderr, "Hash tree scan progress: %d/%d\n", num, denom);

  last_num = num;
  last_denom = denom;
}

static void app_awaiting_stability(praef_app* _, praef_object_id id,
                                   praef_instant systime,
                                   praef_instant committed,
                                   praef_instant validated) {
  static praef_instant last_report = 0;

  if (systime > last_report + 10) {
    fprintf(stderr,
            "Awaiting stability for node %08X "
            "(now = %d, committed = %d, validated = %d).\n",
            id, systime, committed, validated);
    last_report = systime;
  }
}

static void app_information_complete(praef_app* _) {
  fprintf(stderr, "Information download complete.\n");
}

static void app_clock_synced(praef_app* _) {
  fprintf(stderr, "Clocks synchronised.\n");
}

static void app_gained_grant(praef_app* _) {
  fprintf(stderr, "Obtained GRANT.\n");
}

static void app_log(praef_app* _, const char* msg) {
  fprintf(stderr, "%s\n", msg);
}

static void receive_advert(praef_userdata _, const praef_umb_advert* advert,
                           const PraefNetworkIdentifierPair_t* d_netid) {
  unsigned s = advert->sysid, i;

  if (s >= 256) return;
  if (discoveries[s].name[0]) return;
  if (!advert->data_size || advert->data_size > STRMAX) return;
  for (i = 0; i < advert->data_size; ++i)
    if (!isgraph(advert->data[i]))
      return;

  if (!!netid->internet != !!d_netid->internet) return;

  memcpy(discoveries[s].name, advert->data, advert->data_size);
  discoveries[s].name[advert->data_size] = 0;

  discoveries[s].netid = *d_netid;
  if (PraefIpAddress_PR_ipv4 == d_netid->intranet.address.present) {
    memcpy(discoveries[s].netid_local_addr,
           d_netid->intranet.address.choice.ipv4.buf,
           d_netid->intranet.address.choice.ipv4.size);
    discoveries[s].netid.intranet.address.choice.ipv4.buf =
      discoveries[s].netid_local_addr;
  } else {
    memcpy(discoveries[s].netid_local_addr,
           d_netid->intranet.address.choice.ipv6.buf,
           d_netid->intranet.address.choice.ipv6.size);
    discoveries[s].netid.intranet.address.choice.ipv6.buf =
      discoveries[s].netid_local_addr;
  }
  if (d_netid->internet) {
    discoveries[s].netid_internet = *d_netid->internet;
    discoveries[s].netid.internet = &discoveries[s].netid_internet;
    if (PraefIpAddress_PR_ipv4 == d_netid->internet->address.present) {
      memcpy(discoveries[s].netid_global_addr,
             d_netid->internet->address.choice.ipv4.buf,
             d_netid->internet->address.choice.ipv4.size);
      discoveries[s].netid.internet->address.choice.ipv4.buf =
        discoveries[s].netid_global_addr;
    } else {
      memcpy(discoveries[s].netid_global_addr,
             d_netid->internet->address.choice.ipv6.buf,
             d_netid->internet->address.choice.ipv6.size);
      discoveries[s].netid.internet->address.choice.ipv6.buf =
        discoveries[s].netid_global_addr;
    }
  }

  fprintf(stderr, "Discovered system %d with name %s at ",
          s, discoveries[s].name);
  print_netid(&discoveries[s].netid);
  fprintf(stderr, "\n");
}

static void configure_advert(unsigned sysid, const char* name) {
  praef_umb_advert advert;

  advert.sysid = sysid;
  strlcpy((char*)advert.data, name, sizeof(advert.data));
  advert.data_size = strlen((char*)advert.data);

  praef_umb_set_advert(bus, &advert);
}

static void connect_bootstrap(const char* name) {
  unsigned sysid = time(NULL) & 0xFF;

  fprintf(stderr, "Bootstrapping system with id %d.\n", sysid);
  configure_advert(sysid, name);
  praef_system_bootstrap(sys);
}

static void connect_discover(const char* name) {
  unsigned ticks, i;

  praef_umb_set_listen_advert(bus, receive_advert, NULL);

  for (ticks = 0; ticks < 100; ++ticks) {
    if (0 == ticks % 10) {
      if (praef_umb_send_discovery(bus))
        err(EX_OSERR, "Failed to send discovery packet");

      fprintf(stderr, "Sent Discover\n");
    }

    delay();
    pump_network();

    for (i = 0; i < 256; ++i) {
      if (!strcmp(name, discoveries[i].name)) {
        fprintf(stderr, "Connecting to system id %d.\n", i);
        praef_umb_set_listen_advert(bus, NULL, NULL);
        configure_advert(i, name);
        praef_system_connect(sys, &discoveries[i].netid);
        return;
      }
    }
  }

  errx(EX_NOHOST, "%s not discovered after 10 tries; giving up.", name);
}

static void enable_broadcast(void) {
  if (praef_umb_set_use_broadcast(bus, 1))
    err(EX_OSERR, "Failed to enable broadcasting");

  fprintf(stderr, "Broadcasting enabled.\n");
}

static void set_vertex(const char* host, const char* sport) {
  unsigned short port = atoi(sport);
  unsigned tries;

  if (!port)
    errx(EX_USAGE, "Invalid vertex port: %s", sport);

  if (praef_umb_lookup_vertex(bus, host, port))
    err(EX_NOHOST, "Failed to lookup vertex at %s:%d", host, (unsigned)port);

  praef_umb_set_use_vertex(bus, 1);

  fprintf(stderr, "Waiting for vertex to provide global address...\n");
  for (tries = 0; tries < 100; ++tries) {
    delay();
    pump_network();
    if (praef_umb_global_address(bus)) {
      netid = praef_umb_global_address(bus);
      fprintf(stderr, "Global identifier is ");
      print_netid(netid);
      fprintf(stderr, "\n");

      praef_umb_set_use_vertex(bus, 1);
      return;
    }
  }

  errx(EX_NOHOST, "Failed to receive a response from vertex for 10s");
}

static void enable_local_listen(void) {
  praef_umb_set_listen_discover(bus, 1);
  fprintf(stderr, "Listening on local link for Discover messages.\n");
}

static void enable_firewall_spam(void) {
  if (praef_umb_set_spam_firewall(bus, 1))
    err(EX_OSERR, "Failed to configure firewall spamming");

  fprintf(stderr, "Firewall spamming enabled.");
}

static void pump_network(void) {
  unsigned char data[512];

  while ((*bus->recv)(data, sizeof(data), bus));
}

static void delay(void) {
  SDL_Delay(100);
}

static void run(void) {
  char line[STRMAX+1];

#ifdef HAVE_FCNTL_H
  if (fcntl(0, F_SETFL, O_NONBLOCK))
    warn("Failed to set NONBLOCK on stdin");
#else
  warn("fcntl() not available. You'll have to hold enter for "
       "the program to advance.");
#endif

  while (!feof(stdin)) {
    if (fgets(line, sizeof(line), stdin) &&
        praef_system_get_local_id(sys))
      oom_if_not(praef_system_add_event(sys, line, strlen(line)-1));

    praef_system_advance(sys, 1);
    delay();
  }
}
