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

#include <string.h>
#include <assert.h>

#include "ack-table.h"

static int praef_ack_in_range(praef_advisory_serial_number base,
                              praef_advisory_serial_number other) {
  unsigned offset = other - base;

  return offset < PRAEF_ACK_TABLE_SIZE;
}

void praef_ack_local_init(praef_ack_local* this) {
  memset(this, 0, sizeof(praef_ack_local));
}

void praef_ack_local_put(praef_ack_local* this, const praef_hlmsg* msg) {
  praef_advisory_serial_number sn = praef_hlmsg_serno(msg);
  unsigned offset = sn - this->base, shunted, i;

  /* Shunt base if necessary */
  if (offset >= PRAEF_ACK_TABLE_SIZE) {
    shunted = offset - PRAEF_ACK_TABLE_SIZE + 1;

    if (shunted < PRAEF_ACK_TABLE_SIZE) {
      /* Table partially invalidated */
      for (i = 0; i < shunted; ++i)
        this->received[(i + this->base) & PRAEF_ACK_TABLE_MASK] = NULL;
    } else {
      /* Table fully invalidated. */
      memset(this->received, 0, sizeof(this->received));
    }

    this->base += shunted;
    if (!praef_ack_in_range(this->base, this->delta_start))
      this->delta_start = this->base;
    if (!praef_ack_in_range(this->base+1, this->delta_end))
      this->delta_end = this->base;
  }

  if (!this->received[sn & PRAEF_ACK_TABLE_MASK]) {
    this->received[sn & PRAEF_ACK_TABLE_MASK] = msg;

    if (!praef_ack_in_range(this->delta_start, sn))
      this->delta_start = sn;
    if (!praef_ack_in_range(sn+1, this->delta_end))
      this->delta_end = sn+1;
  }
}

void praef_ack_remote_init(praef_ack_remote* this) {
  memset(this, 0, sizeof(praef_ack_remote));
}

void praef_ack_remote_set_base(praef_ack_remote* this,
                               praef_advisory_serial_number log_base,
                               unsigned neg_off,
                               unsigned min_len) {
  praef_advisory_serial_number new_base;
  unsigned shunted, i;

  if (neg_off <= PRAEF_ACK_TABLE_SIZE - min_len) {
    new_base = log_base - neg_off;
  } else {
    new_base = log_base - PRAEF_ACK_TABLE_SIZE + min_len;
  }

  shunted = new_base - this->base;

  if (shunted < PRAEF_ACK_TABLE_SIZE) {
    /* Partial table invalidation */
    for (i = 0; i < shunted; ++i)
      this->received[(i + this->base) & PRAEF_ACK_TABLE_MASK] = praef_are_unk;
  } else {
    /* Total table invalidation */
    memset(this->received, 0, sizeof(this->received));
  }

  this->base = new_base;
}

void praef_ack_remote_put(praef_ack_remote* this,
                          praef_advisory_serial_number sn,
                          int received) {
  assert(praef_ack_in_range(this->base, sn));

  if (praef_are_ack != this->received[sn & PRAEF_ACK_TABLE_MASK])
    this->received[sn & PRAEF_ACK_TABLE_MASK] =
      received? praef_are_ack : praef_are_nak;
}

unsigned praef_ack_find_missing(const praef_hlmsg* dst[PRAEF_ACK_TABLE_SIZE],
                                const praef_ack_local* local,
                                const praef_ack_remote* remote) {
  praef_advisory_serial_number sn;
  unsigned ix = 0;

  /* The starting index is the sequentially greater of the local and remote
   * bases. If both are equal, both are in range of each other, but it doesn't
   * matter which one we use. If neither is in range of the other, there's no
   * meaningful comparison to do anyway, so just return in that case. In any
   * other case, the base that is within range of the other is the sequentially
   * greater.
   */
  if (praef_ack_in_range(local->base, remote->base))
    sn = remote->base;
  else
    /* This might not be in range of remoe->base either, but in that case the
     * below loop just exits immediately anyway.
     */
    sn = local->base;

  while (praef_ack_in_range(local->base, sn) &&
         praef_ack_in_range(remote->base, sn)) {
    if (local->received[sn & PRAEF_ACK_TABLE_MASK] &&
        praef_are_nak == remote->received[sn & PRAEF_ACK_TABLE_MASK])
      dst[ix++] = local->received[sn & PRAEF_ACK_TABLE_MASK];
  }

  return ix;
}
