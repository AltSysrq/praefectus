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
#ifndef LIBPRAEFECTUS__SYSTEM_ROUTEMGR_H_
#define LIBPRAEFECTUS__SYSTEM_ROUTEMGR_H_

#include "system.h"
#include "messages/PraefMsgPing.h"
#include "messages/PraefMsgPong.h"
#include "messages/PraefMsgRoute.h"

typedef struct {
  unsigned ungranted_route_interval;
  unsigned granted_route_interval;
  unsigned ping_interval;
  unsigned max_pong_silence;
  unsigned route_kill_delay;

  unsigned char ping_salt[8];
} praef_system_routemgr;

#define PRAEF_NODE_ROUTEMGR_NUM_LATENCY_SAMPLES 8

typedef struct {
  int has_route;
  praef_instant last_route_message;
  praef_instant last_ping;
  praef_instant last_pong;
  int in_flight_ping;
  unsigned latency;
  unsigned latency_samples[PRAEF_NODE_ROUTEMGR_NUM_LATENCY_SAMPLES];
  PraefPingId_t current_ping_id;
} praef_node_routemgr;

int praef_system_routemgr_init(praef_system*);
void praef_system_routemgr_destroy(praef_system*);

int praef_node_routemgr_init(struct praef_node_s*);
void praef_node_routemgr_destroy(struct praef_node_s*);
void praef_node_routemgr_update(struct praef_node_s*, unsigned);

void praef_system_routemgr_recv_msg_route(
  praef_system*, const PraefMsgRoute_t*);
void praef_node_routemgr_recv_msg_ping(
  struct praef_node_s*, const PraefMsgPing_t*);
void praef_node_routemgr_recv_msg_pong(
  struct praef_node_s*, const PraefMsgPong_t*);

#endif /* LIBPRAEFECTUS__SYSTEM_ROUTEMGR_H_ */
