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

#include <assert.h>

#include "-system.h"
#include "messages/PraefMsg.h"

void praef_system_conf_propose_grant_interval(praef_system* sys, unsigned i) {
  sys->mod.propose_grant_interval = i;
}

void praef_system_conf_vote_deny_interval(praef_system* sys, unsigned i) {
  sys->mod.vote_deny_interval = i;
}

void praef_system_conf_vote_chmod_offset(praef_system* sys, unsigned i) {
  sys->mod.vote_chmod_offset = i;
}

int praef_system_mod_init(praef_system* sys) {
  sys->mod.propose_grant_interval = 16 * sys->std_latency;
  sys->mod.vote_deny_interval = 16 * sys->std_latency;
  sys->mod.vote_chmod_offset = 16 * sys->std_latency;
  return 1;
}

void praef_system_mod_destroy(praef_system* sys) { }

int praef_node_mod_init(praef_node* node) {
  return 1;
}

void praef_node_mod_destroy(praef_node* node) { }

void praef_system_mod_update(praef_system* sys) {
  PraefMsg_t msg;

  if (!sys->local_node) return;

  if (praef_node_has_grant(sys->local_node)) {
    if (praef_sjs_requesting_grant == sys->join_state) {
      ++sys->join_state;

      if (PRAEF_APP_HAS(sys->app, gained_grant_opt))
        (*sys->app->gained_grant_opt)(sys->app);
    }
  } else {
    if (sys->clock.ticks - sys->mod.last_grant_proposal >=
        sys->mod.propose_grant_interval) {
      memset(&msg, 0, sizeof(msg));
      msg.present = PraefMsg_PR_chmod;
      msg.choice.chmod.node = sys->local_node->id;
      msg.choice.chmod.effective =
        praef_outbox_get_now(sys->router.cr_out) +
        sys->mod.vote_chmod_offset;
      msg.choice.chmod.bit = PraefMsgChmod__bit_grant;
      PRAEF_OOM_IF_NOT(sys, praef_outbox_append(
                         sys->router.cr_out, &msg));
      sys->mod.last_grant_proposal = sys->clock.ticks;
    }
  }
}

void praef_node_mod_update(praef_node* node) {
  PraefMsg_t msg;

  /* Deliberately include the local node as a possibility; this is how graceful
   * disconnect works.
   */
  if (praef_nd_negative == node->disposition && !praef_node_has_deny(node)) {
    praef_instant vote_to_deny_at = node->sys->clock.systime
      / node->sys->mod.vote_deny_interval * node->sys->mod.vote_deny_interval
      + node->sys->mod.vote_chmod_offset;

    if (vote_to_deny_at > node->mod.last_deny_vote) {
      memset(&msg, 0, sizeof(msg));
      msg.present = PraefMsg_PR_chmod;
      msg.choice.chmod.node = node->id;
      msg.choice.chmod.effective = vote_to_deny_at;
      msg.choice.chmod.bit = PraefMsgChmod__bit_deny;
      PRAEF_OOM_IF_NOT(node->sys, praef_outbox_append(
                         node->sys->router.cr_out, &msg));
      node->mod.last_deny_vote = vote_to_deny_at;
    }
  }
}

static int praef_system_mod_is_chmod_permissible(
  praef_system* sys,
  praef_instant envelope_instant, const PraefMsgChmod_t* msg
) {
  return envelope_instant <= msg->effective &&
    msg->effective - envelope_instant <= sys->mod.vote_chmod_offset;
}

void praef_node_mod_recv_msg_chmod(praef_node* node,
                                   praef_instant envelope_instant,
                                   const PraefMsgChmod_t* msg) {
  PraefMsg_t echo;
  praef_node* target;

  /* Ensure the time is within acceptable bounds */
  if (!praef_system_mod_is_chmod_permissible(
        node->sys, envelope_instant, msg)) {
    /* Out of bounds. Ignore the request and exact revenge. */
    praef_node_negative(node, "Sent non-permissible chmod");
    return;
  }

  target = praef_system_get_node(node->sys, msg->node);
  assert(target);

  /* OK. Pass on to the lower system. If we agree with the vote, echo it if we
   * haven't already done so.
   */
  (*node->sys->app->chmod_bridge)(node->sys->app,
                                  msg->node,
                                  node->id,
                                  1 << msg->bit,
                                  msg->effective);
  if (!(*node->sys->app->has_chmod_bridge)(
        node->sys->app,
        msg->node,
        node->sys->local_node->id,
        1 << msg->bit,
        msg->effective)) {
    /* Not yet present, echo if we agree and if we won't be violating time
     * constraints in doing so.
     */
    if ((praef_nd_negative == target->disposition &&
         PraefMsgChmod__bit_deny == msg->bit &&
         !praef_node_has_deny(target)) ||
        (praef_nd_positive == target->disposition &&
         PraefMsgChmod__bit_grant == msg->bit &&
         !praef_node_has_grant(target))) {
      memset(&echo, 0, sizeof(echo));
      echo.present = PraefMsg_PR_chmod;
      memcpy(&echo.choice.chmod, msg, sizeof(PraefMsgChmod_t));

      if (praef_system_mod_is_chmod_permissible(
            node->sys, praef_outbox_get_now(node->sys->router.cr_out),
            &echo.choice.chmod)) {
        PRAEF_OOM_IF_NOT(node->sys, praef_outbox_append(
                           node->sys->router.cr_out, &echo));

        /* Immediately loop it back into this function, so that multiple
         * identical votes from this frame don't result in duplicate outbound
         * messages (which could snowball).
         */
        praef_node_mod_recv_msg_chmod(
          node->sys->local_node,
          praef_outbox_get_now(node->sys->router.cr_out),
          &echo.choice.chmod);
      }
    }
  }
}
