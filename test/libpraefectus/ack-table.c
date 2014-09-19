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

#include "test.h"

#include <libpraefectus/ack-table.h>
#include <libpraefectus/hl-msg.h>
#include <libpraefectus/messages/PraefMsg.h>

/* Note: These tests leak memory, since nothing they're actually testing
 * involves dynamic memory allocation itself.
 */

static praef_ack_local local;
static praef_ack_remote remote;

static praef_hlmsg_encoder* encoder;
static praef_advisory_serial_number encoder_serno;

defsuite(libpraefectus_ack_table);

defsetup {
  praef_ack_local_init(&local);
  praef_ack_remote_init(&remote);
  encoder = praef_hlmsg_encoder_new(
    praef_htf_rpc_type, NULL, &encoder_serno,
    PRAEF_HLMSG_MTU_MIN, 0);
}

defteardown {
  unsigned i;

  /* Validate invariants on local */
  for (i = 0; i < PRAEF_ACK_TABLE_SIZE; ++i)
    if (local.received[(i + local.base) & PRAEF_ACK_TABLE_MASK])
      ck_assert_int_eq(i + local.base,
                       praef_hlmsg_serno(
                         local.received[(i + local.base) &
                                        PRAEF_ACK_TABLE_MASK]));
}

static praef_hlmsg* mkmsg(praef_advisory_serial_number serno) {
  PraefMsg_t msg;
  static unsigned char buf[PRAEF_HLMSG_MTU_MIN];
  praef_hlmsg* hlmsg;

  encoder_serno = serno;
  msg.present = PraefMsg_PR_ping;
  msg.choice.ping.id = 0;

  hlmsg = praef_hlmsg_of(buf, sizeof(buf));
  praef_hlmsg_encoder_singleton(hlmsg, encoder, &msg);
  return hlmsg;
}

deftest(local_can_place_messages_in_initial_range) {
  praef_ack_local_put(&local, mkmsg(42));
  praef_ack_local_put(&local, mkmsg(33));
  praef_ack_local_put(&local, mkmsg(100));

  ck_assert_int_eq(0, local.base);
  ck_assert_int_eq(0, local.delta_start);
  ck_assert_int_eq(101, local.delta_end);
  ck_assert_ptr_ne(NULL, local.received[42]);
  ck_assert_ptr_ne(NULL, local.received[33]);
  ck_assert_ptr_ne(NULL, local.received[100]);
}

deftest(local_put_shunts_range_minimally) {
  praef_ack_local_put(&local, mkmsg(1));
  praef_ack_local_put(&local, mkmsg(PRAEF_ACK_TABLE_SIZE));

  ck_assert_int_eq(1, local.base);
  ck_assert_int_eq(1, local.delta_start);
  ck_assert_int_eq(PRAEF_ACK_TABLE_SIZE+1, local.delta_end);
  ck_assert_ptr_ne(NULL, local.received[0]);
  ck_assert_ptr_ne(NULL, local.received[1]);
}

deftest(local_put_shunt_clears_table_partially) {
  praef_ack_local_put(&local, mkmsg(42));
  praef_ack_local_put(&local, mkmsg(100));
  praef_ack_local_put(&local, mkmsg(PRAEF_ACK_TABLE_SIZE+54));

  ck_assert_int_eq(55, local.base);
  ck_assert_int_eq(55, local.delta_start);
  ck_assert_int_eq(PRAEF_ACK_TABLE_SIZE+55, local.delta_end);
  ck_assert_ptr_eq(NULL, local.received[42]);
  ck_assert_ptr_ne(NULL, local.received[100]);
  ck_assert_ptr_ne(NULL, local.received[54]);
}

deftest(local_put_shunt_clears_table_totally) {
  praef_ack_local_put(&local, mkmsg(1));
  praef_ack_local_put(&local, mkmsg(~0u));

  ck_assert_int_eq((unsigned)(0 - PRAEF_ACK_TABLE_SIZE), local.base);
  ck_assert_int_eq((unsigned)(0 - PRAEF_ACK_TABLE_SIZE), local.delta_start);
  ck_assert_int_eq(0, local.delta_end);
  ck_assert_ptr_ne(NULL, local.received[PRAEF_ACK_TABLE_MASK]);
  ck_assert_ptr_eq(NULL, local.received[1]);
}

deftest(local_put_will_retreaet_delta_start) {
  local.delta_start = 42;
  local.delta_end = 43;
  praef_ack_local_put(&local, mkmsg(1));

  ck_assert_ptr_eq(0, local.base);
  ck_assert_int_eq(1, local.delta_start);
  ck_assert_int_eq(43, local.delta_end);
}

deftest(remote_set_base_uses_negative_offset_when_possible) {
  praef_ack_remote_set_base(&remote, 42, 10, 64);

  ck_assert_int_eq(32, remote.base);
}

deftest(remote_set_base_ignores_negative_offset_when_impossible) {
  praef_ack_remote_set_base(&remote, 42, 10, PRAEF_ACK_TABLE_SIZE-2);

  ck_assert_int_eq(40, remote.base);
}

deftest(remote_set_base_partially_invalidates_table) {
  remote.received[0] = praef_are_ack;
  remote.received[1] = praef_are_ack;

  praef_ack_remote_set_base(&remote, 1, 0, 0);
  ck_assert_int_eq(1, remote.base);
  ck_assert_int_eq(praef_are_unk, remote.received[0]);
  ck_assert_int_eq(praef_are_ack, remote.received[1]);
}

deftest(remote_put_sets_stati_correctly) {
  remote.received[1] = praef_are_ack;
  remote.received[2] = praef_are_nak;
  remote.received[4] = praef_are_ack;
  remote.received[5] = praef_are_nak;

  praef_ack_remote_put(&remote, 0, 0);
  praef_ack_remote_put(&remote, 1, 0);
  praef_ack_remote_put(&remote, 2, 0);
  praef_ack_remote_put(&remote, 3, 1);
  praef_ack_remote_put(&remote, 4, 1);
  praef_ack_remote_put(&remote, 5, 1);

  ck_assert_int_eq(praef_are_nak, remote.received[0]);
  ck_assert_int_eq(praef_are_ack, remote.received[1]);
  ck_assert_int_eq(praef_are_nak, remote.received[2]);
  ck_assert_int_eq(praef_are_ack, remote.received[3]);
  ck_assert_int_eq(praef_are_ack, remote.received[4]);
  ck_assert_int_eq(praef_are_ack, remote.received[5]);
}

deftest(find_missing_same_range) {
  const praef_hlmsg* missing[PRAEF_ACK_TABLE_SIZE];
  unsigned count;

  praef_ack_local_put(&local, mkmsg(0));
  praef_ack_local_put(&local, mkmsg(1));
  praef_ack_local_put(&local, mkmsg(2));
  praef_ack_remote_put(&remote, 0, 1);
  praef_ack_remote_put(&remote, 1, 0);
  praef_ack_remote_put(&remote, 3, 1);
  praef_ack_remote_put(&remote, 4, 0);

  count = praef_ack_find_missing(missing, &local, &remote);
  ck_assert_int_eq(1, count);
  ck_assert_int_eq(1, praef_hlmsg_serno(missing[0]));
}

deftest(find_missing_remote_at_tail_of_local) {
  const praef_hlmsg* missing[PRAEF_ACK_TABLE_SIZE];
  unsigned count;

  praef_ack_local_put(&local, mkmsg(0));
  praef_ack_local_put(&local, mkmsg(1));
  praef_ack_local_put(&local, mkmsg(PRAEF_ACK_TABLE_SIZE-1));
  ck_assert_int_eq(0, local.base);
  praef_ack_remote_set_base(&remote, PRAEF_ACK_TABLE_SIZE - 1, 0, 2);
  praef_ack_remote_put(&remote, PRAEF_ACK_TABLE_SIZE-1, 0);
  praef_ack_remote_put(&remote, PRAEF_ACK_TABLE_SIZE, 0);

  count = praef_ack_find_missing(missing, &local, &remote);
  ck_assert_int_eq(1, count);
  ck_assert_int_eq(PRAEF_ACK_TABLE_SIZE-1, praef_hlmsg_serno(missing[0]));
}

deftest(find_missing_local_at_tail_of_remote) {
  const praef_hlmsg* missing[PRAEF_ACK_TABLE_SIZE];
  unsigned count;

  local.base = PRAEF_ACK_TABLE_SIZE-1;
  praef_ack_local_put(&local, mkmsg(PRAEF_ACK_TABLE_SIZE-1));
  praef_ack_local_put(&local, mkmsg(PRAEF_ACK_TABLE_SIZE));
  ck_assert_ptr_ne(NULL, local.received[0]);
  praef_ack_remote_put(&remote, 0, 0);
  praef_ack_remote_put(&remote, PRAEF_ACK_TABLE_SIZE-1, 0);

  count = praef_ack_find_missing(missing, &local, &remote);
  ck_assert_int_eq(1, count);
  ck_assert_int_eq(PRAEF_ACK_TABLE_SIZE-1, praef_hlmsg_serno(missing[0]));
}

deftest(find_missing_disjoint_ranges) {
  const praef_hlmsg* missing[PRAEF_ACK_TABLE_SIZE];
  unsigned count;

  local.base = PRAEF_ACK_TABLE_SIZE;
  praef_ack_local_put(&local, mkmsg(PRAEF_ACK_TABLE_SIZE));
  ck_assert_ptr_ne(NULL, local.received[0]);
  praef_ack_remote_put(&remote, 0, 0);

  count = praef_ack_find_missing(missing, &local, &remote);
  ck_assert_int_eq(0, count);
}
