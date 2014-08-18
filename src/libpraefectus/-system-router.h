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
#ifndef LIBPRAEFECTUS__SYSTEM_ROUTER_H_
#define LIBPRAEFECTUS__SYSTEM_ROUTER_H_

#include "system.h"
#include "outbox.h"
#include "hl-msg.h"

struct praef_node_s;

typedef struct {
  praef_advisory_serial_number adv_serno;

  /* Outboxen for the two types of redistributable messages.
   * Uncommitted-redistributable messages can be broadcast, and thus use one mq
   * for the whole system. Comitted-redistributable messages must be delayable,
   * and thus have a separate mq per node.
   *
   * Note that the ur_out is tied not only to the ur_mq (which broadcasts to
   * the main message bus) but also to the loopback bus used by the state
   * manager. The same goes for cr_out.
   */
  praef_outbox* cr_out, * ur_out;
  praef_mq* ur_mq;
} praef_system_router;

typedef struct {
  praef_outbox* rpc_out;
  praef_mq* cr_mq, * rpc_mq;
} praef_node_router;

int praef_system_router_init(praef_system*);
void praef_system_router_destroy(praef_system*);
void praef_system_router_update(praef_system*, unsigned);

int praef_node_router_init(struct praef_node_s*);
void praef_node_router_destroy(struct praef_node_s*);
void praef_node_router_update(struct praef_node_s*, unsigned);

#endif /* LIBPRAEFECTUS__SYSTEM_ROUTER_H_ */
