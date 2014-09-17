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
#ifndef LIBPRAEFECTUS__SYSTEM_COMMIT_H_
#define LIBPRAEFECTUS__SYSTEM_COMMIT_H_

#include "system.h"
#include "commitment-chain.h"
#include "messages/PraefMsgCommit.h"

typedef struct {
  unsigned commit_interval;
  unsigned max_commit_lag;
  unsigned max_validated_lag;
  unsigned commit_lag_laxness;
  unsigned self_commit_lag_compensation_16;

  praef_comchain* commit_builder;
  praef_instant last_commit;
} praef_system_commit;

typedef struct {
  praef_comchain* comchain;
} praef_node_commit;

int praef_system_commit_init(praef_system*);
void praef_system_commit_destroy(praef_system*);
void praef_system_commit_update(praef_system*);

int praef_node_commit_init(struct praef_node_s*);
void praef_node_commit_destroy(struct praef_node_s*);
void praef_node_commit_update(struct praef_node_s*);

praef_instant praef_node_visibility_threshold(struct praef_node_s*);

void praef_node_commit_observe_message(struct praef_node_s*,
                                       praef_instant,
                                       const unsigned char[PRAEF_HASH_SIZE]);
void praef_node_commit_recv_msg_commit(struct praef_node_s*,
                                       praef_instant,
                                       const PraefMsgCommit_t*);

#endif /* LIBPRAEFECTUS__SYSTEM_COMMIT_H_ */
