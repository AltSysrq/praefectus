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
#ifndef LIBPRAEFECTUS__SYSTEM_ACK_H_
#define LIBPRAEFECTUS__SYSTEM_ACK_H_

#include "system.h"
#include "ack-table.h"
#include "messages/PraefMsg.h"

typedef struct {
  unsigned direct_ack_interval;
  unsigned indirect_ack_interval;

  praef_instant last_direct_ack;
  praef_instant last_indirect_ack;
} praef_system_ack;

typedef struct {
  /* Received messages are written to a direct and indirect local ack
   * table. The latter is used only to create indirect Received messages.
   *
   * We keep full state on Received messages against the local node from this
   * node; remote-remote queries statelessly compare against the target node's
   * al_direct.
   */
  praef_ack_local al_direct, al_indirect;
  praef_ack_remote ack_remote;
} praef_node_ack;

int praef_system_ack_init(praef_system*);
void praef_system_ack_destroy(praef_system*);
void praef_system_ack_update(praef_system*);

int praef_node_ack_init(struct praef_node_s*);
void praef_node_ack_destroy(struct praef_node_s*);
void praef_node_ack_update(struct praef_node_s*);

void praef_node_ack_observe_msg(struct praef_node_s*,
                                const praef_hlmsg*);
void praef_node_ack_recv_msg_received(struct praef_node_s*,
                                      const PraefMsgReceived_t*);

#endif /* LIBPRAEFECTUS__SYSTEM_ACK_H_ */
