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
#ifndef LIBPRAEFECTUS__SYSTEM_MOD_H_
#define LIBPRAEFECTUS__SYSTEM_MOD_H_

#include "system.h"
#include "messages/PraefMsgChmod.h"

typedef struct {
  unsigned propose_grant_interval;
  unsigned vote_deny_interval;
  unsigned vote_chmod_offset;

  praef_instant last_grant_proposal;
} praef_system_mod;

typedef struct {
  praef_instant last_deny_vote;
} praef_node_mod;

int praef_system_mod_init(praef_system*);
void praef_system_mod_destroy(praef_system*);
void praef_system_mod_update(praef_system*, unsigned);

int praef_node_mod_init(struct praef_node_s*);
void praef_node_mod_destroy(struct praef_node_s*);
void praef_node_mod_update(struct praef_node_s*, unsigned);

void praef_node_mod_recv_msg_chmod(struct praef_node_s*,
                                   praef_instant envelope_instant,
                                   const PraefMsgChmod_t*);

#endif /* LIBPRAEFECTUS__SYSTEM_MOD_H_ */
