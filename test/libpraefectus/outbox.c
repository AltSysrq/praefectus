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

#include <string.h>

#include <libpraefectus/outbox.h>
#include <libpraefectus/message-bus.h>
#include <libpraefectus/hl-msg.h>
#include <libpraefectus/messages/PraefMsg.h>

defsuite(libpraefectus_outbox);

static praef_message_bus bus0, bus1;
static praef_outbox* outbox;
static praef_mq* mq0, * mq1;
static PraefMsg_t msg = {
  .present = PraefMsg_PR_ping,
  .choice = {
    .ping = {
      .id = 42
    }
  }
};

static praef_hlmsg payload;

/* This only works based on knowledge of the implementation. */
static void hlmsg(const void* data, size_t size) {
  payload.data = data;
  payload.size = size+1;
}

defsetup {
  memset(&bus0, 0, sizeof(praef_message_bus));
  memset(&bus1, 0, sizeof(praef_message_bus));
  mq0 = mq1 = NULL;

  outbox = praef_outbox_new(
    praef_hlmsg_encoder_new(praef_htf_rpc_type,
                            NULL,
                            NULL,
                            PRAEF_HLMSG_MTU_MIN,
                            0),
    PRAEF_HLMSG_MTU_MIN);
}

defteardown {
  if (mq0) praef_mq_delete(mq0);
  if (mq1) praef_mq_delete(mq1);
  praef_outbox_delete(outbox);
}

deftest(message_publishing_with_no_subscribers_is_blackhole) {
  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_append_singleton(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));
}

deftest(can_publish_to_one_undelayed_broadcast_subscriber) {
  unsigned received = 0;

  mq0 = praef_mq_new(outbox, &bus0, NULL);

  praef_outbox_set_now(outbox, 42);
  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_append_singleton(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));
  praef_outbox_set_now(outbox, 50);

  bus0.broadcast = lambdav((praef_message_bus* _,
                            const void* data, size_t size),
                           hlmsg(data, size);
                           ck_assert(praef_hlmsg_is_valid(&payload));
                           ck_assert_int_eq(42, praef_hlmsg_instant(&payload));
                           ++received);
  praef_mq_update(mq0);

  ck_assert_int_eq(2, received);
}

deftest(can_publish_to_two_undelayed_broadcast_subscribers) {
  unsigned received0 = 0, received1 = 0;

  mq0 = praef_mq_new(outbox, &bus0, NULL);
  mq1 = praef_mq_new(outbox, &bus1, NULL);

  praef_outbox_set_now(outbox, 42);
  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_append_singleton(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));
  praef_outbox_set_now(outbox, 50);

  bus0.broadcast = lambdav((praef_message_bus* _,
                            const void* data, size_t size),
                           hlmsg(data, size);
                           ck_assert(praef_hlmsg_is_valid(&payload));
                           ck_assert_int_eq(42, praef_hlmsg_instant(&payload));
                           ++received0);
  praef_mq_update(mq0);
  bus1.broadcast = lambdav((praef_message_bus* _,
                            const void* data, size_t size),
                           hlmsg(data, size);
                           ck_assert(praef_hlmsg_is_valid(&payload));
                           ck_assert_int_eq(42, praef_hlmsg_instant(&payload));
                           ++received1);
  praef_mq_update(mq1);

  ck_assert_int_eq(2, received0);
  ck_assert_int_eq(2, received1);
}

deftest(messages_are_aggregated_by_encoder) {
  unsigned received = 0;

  mq0 = praef_mq_new(outbox, &bus0, NULL);

  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));

  bus0.broadcast = lambdav((praef_message_bus* _,
                            const void* data, size_t size),
                           ++received);
  praef_mq_update(mq0);

  ck_assert_int_eq(1, received);
}

deftest(singletons_are_not_aggregated) {
  unsigned received = 0;

  mq0 = praef_mq_new(outbox, &bus0, NULL);

  ck_assert(praef_outbox_append_singleton(outbox, &msg));
  ck_assert(praef_outbox_append_singleton(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));

  bus0.broadcast = lambdav((praef_message_bus* _,
                            const void* data, size_t size),
                           ++received);
  praef_mq_update(mq0);

  ck_assert_int_eq(2, received);
}

deftest(can_delay_messages) {
  unsigned received = 0;

  mq0 = praef_mq_new(outbox, &bus0, NULL);

  praef_outbox_set_now(outbox, 5);
  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));

  praef_mq_set_threshold(mq0, 4);
  praef_mq_update(mq0);

  praef_mq_set_threshold(mq0, 5);
  bus0.broadcast = lambdav((praef_message_bus* _,
                            const void* data, size_t size),
                           hlmsg(data, size);
                           ck_assert_int_eq(5, praef_hlmsg_instant(&payload));
                           ++received);
  praef_mq_update(mq0);

  ck_assert_int_eq(1, received);
}

deftest(different_subscribers_can_have_different_delays) {
  unsigned received = 0;

  mq0 = praef_mq_new(outbox, &bus0, NULL);
  mq1 = praef_mq_new(outbox, &bus0, NULL);

  praef_outbox_set_now(outbox, 5);
  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));

  praef_mq_set_threshold(mq0, 0);
  praef_mq_set_threshold(mq1, 0);

  bus0.broadcast = lambdav((praef_message_bus* _,
                            const void* data, size_t size),
                           hlmsg(data, size);
                           ck_assert_int_eq(5, praef_hlmsg_instant(&payload));
                           ++received);

  praef_mq_update(mq0);
  praef_mq_update(mq1);
  ck_assert_int_eq(0, received);

  praef_mq_set_threshold(mq0, 5);
  praef_mq_update(mq0);
  praef_mq_update(mq1);
  ck_assert_int_eq(1, received);

  praef_mq_set_threshold(mq1, 5);
  praef_mq_update(mq0);
  praef_mq_update(mq1);
  ck_assert_int_eq(2, received);
}

deftest(delivered_messages_are_cleared) {
  unsigned received = 0;

  mq0 = praef_mq_new(outbox, &bus0, NULL);
  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));
  bus0.broadcast = lambdav((praef_message_bus* _,
                            const void* data, size_t size),
                           ++received);

  praef_mq_update(mq0);
  ck_assert_int_eq(1, received);
  praef_mq_update(mq0);
  ck_assert_int_eq(1, received);
}

deftest(undelivered_messages_are_freed_on_mq_deletion) {
  /* This test is only meaningful with valgrind */
  mq0 = praef_mq_new(outbox, &bus0, NULL);
  praef_outbox_set_now(outbox, 5);
  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));
}

deftest(mq_can_unicast) {
  /* Don't need to initialise, we only care about the address */
  PraefNetworkIdentifierPair_t pair;
  unsigned received = 0;

  mq0 = praef_mq_new(outbox, &bus0, &pair);
  ck_assert(praef_outbox_append(outbox, &msg));
  ck_assert(praef_outbox_flush(outbox));
  bus0.unicast = lambdav((praef_message_bus* _,
                          const PraefNetworkIdentifierPair_t* pnip,
                          const void* data, size_t size),
                         hlmsg(data, size);
                         ck_assert(praef_hlmsg_is_valid(&payload));
                         ck_assert_ptr_eq(&pair, pnip);
                         ++received);

  praef_mq_update(mq0);
  ck_assert_int_eq(1, received);
}

deftest(mq_expands_pending_correctly) {
  unsigned i, received = 0;

  mq0 = praef_mq_new(outbox, &bus0, NULL);
  for (i = 0; i < 100; ++i)
    ck_assert(praef_outbox_append_singleton(outbox, &msg));

  bus0.broadcast = lambdav((praef_message_bus* _,
                            const void* data, size_t size),
                           ++received);
  praef_mq_update(mq0);
  ck_assert_int_eq(100, received);
}
