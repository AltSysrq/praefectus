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

#include "mtt-bridge.h"

static void praef_mtt_bridge_accept(praef_mtt_bridge*, praef_event*);
static void praef_mtt_bridge_redact(praef_mtt_bridge*, praef_event*);
static praef_event* praef_mtt_bridge_node_count_delta(
  praef_mtt_bridge*, signed, praef_instant);

void praef_mtt_bridge_init(praef_mtt_bridge* this, praef_transactor* tx) {
  this->cxn.accept = (praef_metatransactor_cxn_accept_t)praef_mtt_bridge_accept;
  this->cxn.redact = (praef_metatransactor_cxn_redact_t)praef_mtt_bridge_redact;
  this->cxn.node_count_delta = (praef_metatransactor_cxn_node_count_delta_t)
    praef_mtt_bridge_node_count_delta;
  this->tx = tx;
}

static void praef_mtt_bridge_accept(praef_mtt_bridge* this, praef_event* evt) {
  praef_context_add_event(praef_transactor_master(this->tx), evt);
}

static void praef_mtt_bridge_redact(praef_mtt_bridge* this, praef_event* evt) {
  praef_context_redact_event(praef_transactor_master(this->tx),
                             evt->object, evt->instant, evt->serial_number);
}

static praef_event* praef_mtt_bridge_node_count_delta(
  praef_mtt_bridge* this, signed delta, praef_instant instant
) {
  return praef_transactor_node_count_delta(this->tx, delta, instant);
}
