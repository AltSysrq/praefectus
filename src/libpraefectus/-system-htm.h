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
#ifndef LIBPRAEFECTUS__SYSTEM_HTM_H_
#define LIBPRAEFECTUS__SYSTEM_HTM_H_

#include "messages/PraefMsgHtLs.h"
#include "messages/PraefMsgHtDir.h"
#include "messages/PraefMsgHtRead.h"
#include "messages/PraefMsgHtRange.h"
#include "messages/PraefMsgHtRangeNext.h"

struct praef_node_s;

typedef struct {
  praef_instant instant;
  const praef_hash_tree* tree;
} praef_system_htm_snapshot;

typedef struct {
  unsigned range_max;
  unsigned range_query_interval;
  unsigned snapshot_interval;
  unsigned num_snapshots;
  unsigned root_query_interval;
  unsigned root_query_offset;

  praef_instant last_root_query;
  /* Array of snapshots. Some or all may be NULL (ie, the tree inside is
   * NULL). Sorted from most recent to oldest.
   */
  praef_system_htm_snapshot* snapshots;
} praef_system_htm;

typedef struct {
  unsigned range_query_mask, range_query_offset;
  unsigned char next_range_query[PRAEF_HASH_SIZE];
  int has_finished_range_query;
  praef_instant last_range_query;
} praef_node_htm;

int praef_system_htm_init(praef_system*);
void praef_system_htm_destroy(praef_system*);
int praef_node_htm_init(struct praef_node_s*);
void praef_node_htm_destroy(struct praef_node_s*);

void praef_node_recv_msg_htls(struct praef_node_s*,
                              const PraefMsgHtLs_t*);
void praef_node_recv_msg_htdir(struct praef_node_s*,
                               const PraefMsgHtDir_t*);
void praef_node_recv_msg_htread(struct praef_node_s*,
                                const PraefMsgHtRead_t*);
void praef_node_recv_msg_htrange(struct praef_node_s*,
                                 const PraefMsgHtRange_t*);
void praef_node_recv_msg_htrangenext(struct praef_node_s*,
                                     const PraefMsgHtRangeNext_t*);

#endif /* LIBPRAEFECTUS__SYSTEM_HTM_H_ */
