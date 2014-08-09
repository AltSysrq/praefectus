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

#include <stdlib.h>

#include "messages/PraefMsg.h"
#include "dsa.h"
#include "hl-msg.h"
#include "system.h"
#include "-system.h"
#include "-system-dispatch.h"

static void praef_system_dispatch_process_cr(
  praef_system*, praef_extnode*, const praef_hlmsg*);
static void praef_system_dispatch_process_ur(
  praef_system*, praef_extnode*, const praef_hlmsg*);
static void praef_system_dispatch_process_rpc(
  praef_system*, praef_extnode*, const praef_hlmsg*);
static void praef_system_dispatch_one(praef_system*, praef_extnode*,
                                      const praef_hlmsg*, const PraefMsg_t*);

void praef_system_dispatch(praef_system* system, const praef_hlmsg* msg) {
  praef_object_id origin_id;
  praef_extnode* origin;
  const praef_hlmsg_segment* segment;
  PraefMsg_t* element;

  /* If the message is invalid, silently discard.
   *
   * While we *could* attempt to check the signature and start voting to DENY
   * the node of origin, this condition will only occur due to deliberate
   * attacks, which don't benefit from correctly signing the message anyway
   * (since in either case they hit this gate and go no further), so it isn't
   * worth the added complexity.
   */
  if (!praef_hlmsg_is_valid(msg)) return;

  /* Identify the node of origin.
   *
   * We can't immediately reject the packet if the signature does not appear
   * valid, as this could be a new node wishing to join the system.
   */
  origin_id = praef_verifier_verify(system->verifier,
                                    praef_hlmsg_pubkey_hint(msg),
                                    praef_hlmsg_signature(msg),
                                    praef_hlmsg_signable(msg),
                                    praef_hlmsg_signable_sz(msg));
  /* Throw the packet out if the local node is the origin, as there is nothing
   * useful that can come of this.
   */
  if (origin_id && origin_id == system->self_id) return;

  origin = RB_FIND(praef_extnode_map, &system->nodes,
                   (praef_extnode*)&origin_id);

  /* For all messages with a known origin, sample the origin's clock. */
  if (origin)
    praef_clock_source_sample(&origin->clock_source, &system->clock,
                              praef_hlmsg_instant(msg), origin->latency);

  /* Class-based processing */
  switch (praef_hlmsg_type(msg)) {
  case praef_htf_committed_redistributable:
    praef_system_dispatch_process_cr(system, origin, msg);
    break;
  case praef_htf_uncommitted_redistributable:
    praef_system_dispatch_process_ur(system, origin, msg);
    break;
  case praef_htf_rpc_type:
    praef_system_dispatch_process_rpc(system, origin, msg);
    break;
  }

  /* Process each segment */
  for (segment = praef_hlmsg_first(msg); segment;
       segment = praef_hlmsg_snext(segment)) {
    element = praef_hlmsg_sdec(segment);
    praef_system_dispatch_one(system, origin, msg, element);
    (*asn_DEF_PraefMsg.free_struct)(&asn_DEF_PraefMsg, element, 0);
  }
}

static void praef_system_dispatch_process_cr(praef_system* system,
                                             praef_extnode* origin,
                                             const praef_hlmsg* msg) {
  /* TODO */
}

static void praef_system_dispatch_process_ur(praef_system* system,
                                             praef_extnode* origin,
                                             const praef_hlmsg* msg) {
  /* TODO */
}

static void praef_system_dispatch_process_rpc(praef_system* system,
                                              praef_extnode* origin,
                                              const praef_hlmsg* msg) {
  /* TODO */
}

static void praef_system_dispatch_one(praef_system* system,
                                      praef_extnode* origin,
                                      const praef_hlmsg* msg,
                                      const PraefMsg_t* element) {
  switch (element->present) {
  case PraefMsg_PR_NOTHING: abort();

  case PraefMsg_PR_ping:
    /* TODO */
    break;

  case PraefMsg_PR_pong:
    /* TODO */
    break;

  case PraefMsg_PR_getnetinfo:
    /* TODO */
    break;

  case PraefMsg_PR_netinfo:
    /* TODO */
    break;

  case PraefMsg_PR_joinreq:
    /* TODO */
    break;

  case PraefMsg_PR_endorsement:
    /* TODO */
    break;

  case PraefMsg_PR_commandeer:
    /* TODO */
    break;

  case PraefMsg_PR_chmod:
    /* TODO */
    break;

  case PraefMsg_PR_vote:
    /* TODO */
    break;

  case PraefMsg_PR_appevt:
    /* TODO */
    break;

  case PraefMsg_PR_commit:
    /* TODO */
    break;

  case PraefMsg_PR_htls:
    /* TODO */
    break;

  case PraefMsg_PR_htdir:
    /* TODO */
    break;

  case PraefMsg_PR_htread:
    /* TODO */
    break;

  case PraefMsg_PR_htrange:
    /* TODO */
    break;

  case PraefMsg_PR_jointree:
    /* TODO */
    break;

  case PraefMsg_PR_jtentry:
    /* TODO */
    break;

  case PraefMsg_PR_received:
    /* TODO */
    break;

  case PraefMsg_PR_appuni:
    /* TODO */
    break;

  case PraefMsg_PR_route:
    /* TODO */
    break;
  }
}
