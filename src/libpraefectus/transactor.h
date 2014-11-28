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
#ifndef LIBPRAEFECTUS_TRANSACTOR_H_
#define LIBPRAEFECTUS_TRANSACTOR_H_

#include "context.h"
#include "event.h"

__BEGIN_DECLS

/**
 * A transactor is a special context/object pair which manages the acceptance
 * and redaction of events within a slave context.
 *
 * Events destined for the slave context are associated with a deadline and a
 * current number of votes. An event is present in the slave context if either
 * (a) the deadline has not yet been passed, or (b) a number of votes at least
 * 50% of the number of nodes at the time of the event have been added for that
 * event.
 *
 * The context embedded in the transactor as well as the slave of the
 * transactor are generally kept in-sync with respect to their current time,
 * but this is not strictly required.
 *
 * A transactor begins with a node count of 1.
 */
typedef struct praef_transactor_s praef_transactor;

/**
 * Constructs a new transactor with the given slave context. If this call
 * succeds, the slave becomes owned by the transactor, and will be destroyed
 * when the transactor is.
 *
 * @param slave The slave context this transactor shall control.
 * @return The new transactor, or NULL if the call fails.
 */
praef_transactor* praef_transactor_new(praef_context* slave);
/**
 * Frees the resources held by the given transactor, including its slave
 * context.
 */
void praef_transactor_delete(praef_transactor*);

/**
 * Returns the context which is a slave to the given transactor. Events should
 * not be directly inserted into the context, but new objects may be, and the
 * context may be advanced.
 */
praef_context* praef_transactor_slave(const praef_transactor*);
/**
 * Returns the context embedded within the transactor itself. Events produced
 * by the transactor event functions may be inserted or redacted, and the
 * context may be advanced; however, no object modifications should be made.
 */
praef_context* praef_transactor_master(const praef_transactor*);

/**
 * Creates a new transactor event for the given transactor which casts a vote
 * for the slave event identified by the given (object,instant,serial_number)
 * triple. The event occurs at the current time of the inner event.
 *
 * This does not add the event to the transactor context; doing so is the
 * responsibility of the caller.
 *
 * @return The event that was created, or NULL if the call fails.
 */
praef_event* praef_transactor_votefor(praef_transactor*,
                                      praef_object_id evt_object,
                                      praef_instant evt_time,
                                      praef_event_serial_number evt_sn);
/**
 * Creates a new transactor event signaling the addition or removal of some
 * number of nodes, for the purposes of vote counting.
 *
 * This does not add the event to the transactor context; doing so is the
 * responsibility of the caller.
 *
 * @param delta The amount by which the node count is to change.
 * @param when The instant in the transactor context at which this change takes
 * place.
 * @return The event that was created, or NULL if the call fails.
 */
praef_event* praef_transactor_node_count_delta(praef_transactor*,
                                               signed delta,
                                               praef_instant when);
/**
 * Creates a new transactor event arranging for the delivery of an event to the
 * slave context.
 *
 * This does not add the event to the transactor context; doing so is the
 * responsibility of the caller.
 *
 * @param evt The event destined for the slave context. If this call succeds,
 * this event is owned by the returned event.
 * @param optimistic Whether the event can be accepted without sufficient
 * votes. Typically, one want to add a corresponding deadline event (see
 * praef_transactor_deadline()) to eventually require a vote.
 * @return The event that was created, or NULL if the call fails.
 */
praef_event* praef_transactor_put_event(praef_transactor*,
                                        praef_event* evt,
                                        int optimistic);

/**
 * Creates a new transactor event which will negate the `optimistic` flag of an
 * event at a certain point in the future. This only makes sense for optimistic
 * events; behaviour is not defined if used for a pessimistic event.
 *
 * This does not add the event to the transactor context; doing so is the
 * responsibility of the caller.
 *
 * @param evt An event passed into praef_transactor_put_event() with
 * optimistic=1.
 * @param deadline The instant (in the containing context) at which the
 * deadline applies.
 * @return The event that was created, or NULL if the call fails.
 */
praef_event* praef_transactor_deadline(praef_transactor*,
                                       praef_event* evt,
                                       praef_instant deadline);

__END_DECLS

#endif /* LIBPRAEFECTUS_TRANSACTOR_H_ */
