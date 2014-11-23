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

#include <stdio.h>

#include "system.h"
#include "system-stderr-log.h"

static void praef_syslog_acquire_id(praef_app*, praef_object_id);
static void praef_syslog_discover_node(
  praef_app*, const PraefNetworkIdentifierPair_t*, praef_object_id);
static void praef_syslog_remove_node(praef_app*, praef_object_id);
static void praef_syslog_join_tree_traversed(praef_app*);
static void praef_syslog_ht_scan_progress(praef_app*, unsigned, unsigned);
static void praef_syslog_awaiting_stability(praef_app*, praef_object_id,
                                            praef_instant, praef_instant,
                                            praef_instant);
static void praef_syslog_information_complete(praef_app*);
static void praef_syslog_clock_synced(praef_app*);
static void praef_syslog_gained_grant(praef_app*);
static void praef_syslog_log(praef_app*, const char*);
static void praef_syslog_print_netid(const PraefNetworkIdentifierPair_t*);
static void praef_syslog_print_single_netid(const PraefNetworkIdentifier_t*);

void praef_app_log_to_stderr(praef_app* app) {
  app->acquire_id_opt = praef_syslog_acquire_id;
  app->discover_node_opt = praef_syslog_discover_node;
  app->remove_node_opt = praef_syslog_remove_node;
  app->join_tree_traversed_opt = praef_syslog_join_tree_traversed;
  app->ht_scan_progress_opt = praef_syslog_ht_scan_progress;
  app->awaiting_stability_opt = praef_syslog_awaiting_stability;
  app->information_complete_opt = praef_syslog_information_complete;
  app->clock_synced_opt = praef_syslog_clock_synced;
  app->gained_grant_opt = praef_syslog_gained_grant;
  app->log_opt = praef_syslog_log;
}

static void praef_syslog_print_netid(const PraefNetworkIdentifierPair_t* id) {
  if (id->internet) {
    praef_syslog_print_single_netid(id->internet);
    fprintf(stderr, "/");
  }
  praef_syslog_print_single_netid(&id->intranet);
}

static void praef_syslog_print_single_netid(
  const PraefNetworkIdentifier_t* id
) {
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

static void praef_syslog_acquire_id(praef_app* _, praef_object_id id) {
  fprintf(stderr, "Acquired id: %08X\n", id);
}

static void praef_syslog_discover_node(
  praef_app* _, const PraefNetworkIdentifierPair_t* netid,
  praef_object_id id
) {
  fprintf(stderr, "Discovered node: %08X at ", id);
  praef_syslog_print_netid(netid);
  fprintf(stderr, "\n");
}

static void praef_syslog_remove_node(praef_app* _, praef_object_id id) {
  fprintf(stderr, "Removed node: %08X\n", id);
}

static void praef_syslog_join_tree_traversed(praef_app* _) {
  fprintf(stderr, "Join tree traversal completed.\n");
}

static void praef_syslog_ht_scan_progress(praef_app* _, unsigned num,
                                          unsigned denom) {
  static unsigned last_num = 0, last_denom = 0;

  if (num != last_num || denom != last_denom)
    fprintf(stderr, "Hash tree scan progress: %d/%d\n", num, denom);

  last_num = num;
  last_denom = denom;
}

static void praef_syslog_awaiting_stability(praef_app* _, praef_object_id id,
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

static void praef_syslog_information_complete(praef_app* _) {
  fprintf(stderr, "Information download complete.\n");
}

static void praef_syslog_clock_synced(praef_app* _) {
  fprintf(stderr, "Clocks synchronised.\n");
}

static void praef_syslog_gained_grant(praef_app* _) {
  fprintf(stderr, "Obtained GRANT.\n");
}

static void praef_syslog_log(praef_app* _, const char* msg) {
  fprintf(stderr, "%s\n", msg);
}
