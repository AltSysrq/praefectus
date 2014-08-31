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
#ifndef LIBPRAEFECTUS__SYSTEM_STATE_H_
#define LIBPRAEFECTUS__SYSTEM_STATE_H_

#include "system.h"
#include "clock.h"
#include "message-bus.h"
#include "outbox.h"
#include "hash-tree.h"

struct praef_node_s;

typedef struct {
  /* MQ for uncommitted-redistributable messages. There is no separate MQ for
   * comitted-redistributable messages since every node already has its own;
   * the local node's is just hooked up to the loopback bus instead of the main
   * one.
   */
  praef_mq* ur_mq;
  /* A dummy message bus that captures outgoing packets and routes them back
   * into the system state.
   *
   * Note that messages are "looped back" by calling directly into
   * praef_system_* functions when messages are sent, rather than being queued
   * for polling. This object only has the vanilla unicast and broadcast
   * methods implemented.
   */
  praef_message_bus loopback;
  praef_hash_tree* hash_tree;
} praef_system_state;

typedef struct {
  praef_clock_source clock_source;
} praef_node_state;

int praef_system_state_init(praef_system*);
void praef_system_state_destroy(praef_system*);
void praef_system_state_update(praef_system*, unsigned);
void praef_system_state_recv_message(praef_system*, praef_hlmsg*);

int praef_node_state_init(struct praef_node_s*);
void praef_node_state_destroy(struct praef_node_s*);
void praef_node_state_update(struct praef_node_s*, unsigned);

#endif /* LIBPRAEFECTUS__SYSTEM_STATE_H_ */
