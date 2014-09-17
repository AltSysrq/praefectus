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

#include "system.h"
#include "-system.h"
#include "-system-router.h"

int praef_system_router_init(praef_system* sys) {
  return
    (sys->router.cr_out = praef_outbox_new(
      praef_hlmsg_encoder_new(praef_htf_committed_redistributable,
                              sys->signator,
                              &sys->router.adv_serno,
                              sys->mtu, 8), sys->mtu)) &&
    (sys->router.ur_out = praef_outbox_new(
      praef_hlmsg_encoder_new(praef_htf_uncommitted_redistributable,
                              sys->signator,
                              &sys->router.adv_serno,
                              sys->mtu, 0), sys->mtu)) &&
    (sys->router.ur_mq = praef_mq_new(sys->router.ur_out,
                                      sys->bus,
                                      NULL));
}

void praef_system_router_destroy(praef_system* sys) {
  if (sys->router.ur_mq) praef_mq_delete(sys->router.ur_mq);
  if (sys->router.ur_out) praef_outbox_delete(sys->router.ur_out);
  if (sys->router.cr_out) praef_outbox_delete(sys->router.cr_out);
}

int praef_node_router_init(praef_node* node) {
  return
    (node->router.rpc_out = praef_outbox_new(
      praef_hlmsg_encoder_new(praef_htf_rpc_type,
                              node->sys->signator,
                              NULL,
                              node->sys->mtu, 0), node->sys->mtu)) &&
    (node->router.cr_mq = praef_mq_new(node->sys->router.cr_out,
                                       node->bus,
                                       &node->net_id)) &&
    (node->router.rpc_mq = praef_mq_new(node->router.rpc_out,
                                        node->bus,
                                        &node->net_id));
}

void praef_node_router_destroy(praef_node* node) {
  if (node->router.rpc_mq) praef_mq_delete(node->router.rpc_mq);
  if (node->router.cr_mq) praef_mq_delete(node->router.cr_mq);
  if (node->router.rpc_out) praef_outbox_delete(node->router.rpc_out);
}

void praef_system_router_update(praef_system* sys) {
  praef_outbox_set_now(sys->router.cr_out, sys->clock.monotime);
  praef_outbox_set_now(sys->router.ur_out, sys->clock.monotime);
}

void praef_system_router_flush(praef_system* sys) {
  PRAEF_OOM_IF_NOT(sys, praef_outbox_flush(sys->router.cr_out));
  PRAEF_OOM_IF_NOT(sys, praef_outbox_flush(sys->router.ur_out));
  praef_mq_update(sys->router.ur_mq);
}


void praef_node_router_update(praef_node* node) {
  praef_outbox_set_now(node->router.rpc_out, node->sys->clock.monotime);
}

void praef_node_router_flush(praef_node* node) {
  PRAEF_OOM_IF_NOT(node->sys, praef_outbox_flush(node->router.rpc_out));
  praef_mq_update(node->router.rpc_mq);
  praef_mq_update(node->router.cr_mq);
}
