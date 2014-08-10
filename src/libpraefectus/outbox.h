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
#ifndef LIBPRAEFECTUS_OUTBOX_H_
#define LIBPRAEFECTUS_OUTBOX_H_

#include "hl-msg.h"
#include "message-bus.h"
#include "messages/PraefMsg.h"
#include "messages/PraefNetworkIdentifierPair.h"

/**
 * An outbox is a front-end to a praef_hlmsg_encoder which distributes
 * resulting messages to subscribed praef_mq objects.
 */
typedef struct praef_outbox_s praef_outbox;
/**
 * A praef_mq object sends encoded messages received from a praef_outbox to
 * which it subscribes over a message bus, either via unicast or
 * broadcast. Messages after a certain threshold will be delayed until the
 * threshold is advanced beyond them.
 */
typedef struct praef_mq_s praef_mq;

/**
 * Constructs a new praef_outbox using the given message encoder.
 *
 * @param enc The message encoder to use. This call takes ownership of the
 * encoder, even if it fails. If this is NULL, this function does nothing and
 * returns NULL. This is to permit the pattern
 * praef_outbox_new(praef_hlmsg_encoder_new(),mtu).
 * @param mtu The maximum message size to expect to encode. This should be the
 * same mtu used to construct enc.
 * @return The new outbox, or NULL if insufficient memory is available. In the
 * latter case, the call deletes the message encoder.
 */
praef_outbox* praef_outbox_new(praef_hlmsg_encoder* enc,
                               unsigned mtu);
/**
 * Frees the memory held by the given outbox. The outbox MUST have no
 * subscribed mq objects.
 */
void praef_outbox_delete(praef_outbox*);

/**
 * Appends an aggregatable message to the outbox.
 *
 * @return Whether the call succeeds.
 */
int praef_outbox_append(praef_outbox*, const PraefMsg_t*);
/**
 * Appends a non-aggregatable message to the outbox.
 *
 * @return Whether the call succeeds.
 */
int praef_outbox_append_singleton(praef_outbox*, const PraefMsg_t*);
/**
 * Ensures all pending aggregatable messages have been encoded and passed to
 * the subscribed outboxes.
 *
 * @return Whether the call succeeds.
 */
int praef_outbox_flush(praef_outbox*);

/**
 * Updates the concept of "now" for the encoder and all subscribed mqs.
 */
void praef_outbox_set_now(praef_outbox*, praef_instant);

/**
 * Creates a new praef_mq subscribed to the given outbox.
 *
 * @param bus The message bus to use to send messages from this mq. The caller
 * must ensure the pointer remains valid for the lifetime of the mq.
 * @param unicast If non-NULL, the network identifier to perform unicasts
 * on. The caller must ensure this pointer remains valid for the lifetime of
 * the mq. If NULL, outgoing messages will be broadcast.
 * @return The new mq object, which is subscribed to the outbox, or NULL if
 * insufficient memory was available.
 */
praef_mq* praef_mq_new(praef_outbox*,
                       praef_message_bus* bus,
                       const PraefNetworkIdentifierPair_t* unicast);
/**
 * Frees the memory held by the given mq object and unsubscribes it from the
 * parent outbox.
 */
void praef_mq_delete(praef_mq*);
/**
 * Changes the threshold beyond which messages will not be sent.
 */
void praef_mq_set_threshold(praef_mq*, praef_instant);
/**
 * Advances the mq the given number of ticks into the future, sending any
 * messages whose delay has fully elapsed.
 */
void praef_mq_update(praef_mq*);

#endif /* LIBPRAEFECTUS_OUTBOX_H_ */
