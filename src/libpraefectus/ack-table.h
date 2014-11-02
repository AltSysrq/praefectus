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
#ifndef LIBPRAEFECTUS_ACK_TABLE_H_
#define LIBPRAEFECTUS_ACK_TABLE_H_

#include "common.h"
#include "hl-msg.h"

#define PRAEF_ACK_TABLE_SIZE (128*8)
#define PRAEF_ACK_TABLE_MASK (PRAEF_ACK_TABLE_SIZE-1)

__BEGIN_DECLS

/**
 * Tracks messages received by the local node according to their advisory
 * serial numbers. This can be used both for reporting to other nodes what
 * messages were received, as well as comparing the positive and negative
 * acknwoledgements against what the local node has received.
 */
typedef struct {
  /**
   * The subset of messages that have been received. NULL entries indicate that
   * no message within the current range with an appropriate serial number has
   * been received. A present value indicates that such a message has in fact
   * been received; in the case of multiple messages with the same serial
   * number, which one is present is unspecified.
   *
   * The index within this array for any message is derived solely from the
   * bitwise AND of its serial number and PRAEF_ACK_TABLE_MASK.
   *
   * Structurally, this array is a rotating queue. The first element is located
   * at (base & PRAEF_ACK_TABLE_MASK) and has serial number (base). The final
   * element is at ((base + PRAEF_ACK_TABLE_SIZE-1) & PRAEF_ACK_TABLE_MASK) and
   * has serial number (base + PRAEF_ACK_TABLE_SIZE-1). This area is termed the
   * currentn range.
   */
  const praef_hlmsg* received[PRAEF_ACK_TABLE_SIZE];
  /**
   * Backing store for received used internally.
   */
  praef_hlmsg messages[PRAEF_ACK_TABLE_SIZE];
  /**
   * The logically earliest serial number in the received array. In cases of
   * wrap-around, this is not necessarily the _least_ serial number, however.
   */
  praef_advisory_serial_number base;
  /**
   * The serial number of the first slot in the current range that has had a
   * message put into it.
   */
  praef_advisory_serial_number delta_start;
  /**
   * One plus the serial number of the last slot in the current range that has
   * a message put into it. (Note that this may be (base +
   * PRAEF_ACK_TABLE_SIZE).)
   */
  praef_advisory_serial_number delta_end;
} praef_ack_local;

/**
 * Describes the received status of a message serial number as stored in a
 * remote ack table.
 */
typedef enum {
  /**
   * Indicates that no information is currently known about whether the remote
   * node has received a matching message.
   */
  praef_are_unk = 0,
  /**
   * Indicates that the remote node definitely has not received a matching
   * message, as of the last time the node provided an update about its ack
   * table.
   */
  praef_are_nak,
  /**
   * Indicates that the remote node definitely has received a matching
   * message.
   */
  praef_are_ack
} praef_ack_remote_entry;

/**
 * Tracks whether a remote node may have received messages with specific serial
 * numbers.
 */
typedef struct {
  /**
   * Indicates the status of each serial number. This is a rotating array queue
   * with the same behaviour and invariants as praef_ack_local::received.
   */
  praef_ack_remote_entry received[PRAEF_ACK_TABLE_SIZE];
  /**
   * The serial number of the first entry in received.
   */
  praef_advisory_serial_number base;
} praef_ack_remote;

/**
 * Initialises the given praef_ack_local.
 */
void praef_ack_local_init(praef_ack_local*);
/**
 * Inserts a message into the local ack table, adjusting base, delta_start, and
 * delta_end as necessary. If this collides with an existing message, the call
 * has no effect.
 *
 * The hlmsg is shallow-copied into the praef_ack_local. The data it points to
 * must remain valid for the life of the praef_ack_local.
 */
void praef_ack_local_put(praef_ack_local*, const praef_hlmsg*);

/**
 * Returns the hlmsg at the given serial number, or NULL if absent or out of
 * range.
 */
const praef_hlmsg* praef_ack_local_get(const praef_ack_local*,
                                       praef_advisory_serial_number);

/**
 * Initialises the given praef_ack_remote, setting every element to an
 * "unknown" status.
 */
void praef_ack_remote_init(praef_ack_remote*);
/**
 * Changes the base on the given praef_ack_remote. Elements not in the old
 * range are reset to unknown.
 *
 * This assumes that the base is being logically advanced. Retreating it will
 * actually clear the entire received array. This call is designed to be
 * directly passed values from a Received message.
 *
 * @param base The logical base that will be used for subsequent calls to
 * praef_ack_remote_put().
 * @param negative_offset The desired negative offset for the actual base. Ie,
 * barring range restrictions, the real base will be (base-negative_offset).
 * @param minimum_length The positive offset from base that is one beyond the
 * last serial number to be reported by praef_ack_remote_put().
 */
void praef_ack_remote_set_base(praef_ack_remote*,
                               praef_advisory_serial_number base,
                               unsigned negative_offset,
                               unsigned minimum_length);
/**
 * Reports whether the remote node definitely has or has not received a message
 * with the given serial number.
 *
 * @param sn The serial number being reported. This MUST be within the current
 * range.
 * @param received Whether any messages with the given serial number has been
 * received. This call will not change a serial number with a
 * definitely-received status to a definitely-not-received status.
 */
void praef_ack_remote_put(praef_ack_remote*, praef_advisory_serial_number sn,
                          int received);

/**
 * Compares the given local and remote ack tables, populating dst with messages
 * that have been received locally and definitely have not been received
 * remotely.
 *
 * @param dst An array of hlmsgs to fill with entries that the local node may
 * want to retransmit to the remote node. All populated elements are guaranteed
 * to be within the current range of both tables.
 * @return The number of elements in dst which were populated. Elements at or
 * beyond this index have undefined contents.
 */
unsigned praef_ack_find_missing(const praef_hlmsg* dst[PRAEF_ACK_TABLE_SIZE],
                                const praef_ack_local*,
                                const praef_ack_remote*);

__END_DECLS

#endif /* LIBPRAEFECTUS_ACK_TABLE_H_ */
