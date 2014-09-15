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
#ifndef LIBPRAEFECTUS_METATRANSACTOR_H_
#define LIBPRAEFECTUS_METATRANSACTOR_H_

#include "common.h"
#include "event.h"
#include "object.h"

/**
 * The status of a node in a metatransactor is a two-bit field consisting of a
 * GRANT and a DENY bit. Events from a node are meaningful only if it has the
 * GRANT bit set and the DENY bit clear. Meta-events can attempt to set either
 * of these bits, but not clear them. This means the two possible lifecycles
 * for a node is UNBORN->ALIVE->KILLED and UNBORN->STILLBORN->KILLED. ALIVE is
 * the only state in which a node can introduce new events.
 */
typedef unsigned char praef_metatransactor_node_status;
#define PRAEF_METATRANSACTOR_NS_GRANT  ((praef_metatransactor_node_status)0x01)
#define PRAEF_METATRANSACTOR_NS_DENY   ((praef_metatransactor_node_status)0x02)
#define PRAEF_METATRANSACTOR_NS_UNBORN ((praef_metatransactor_node_status)0x00)
#define PRAEF_METATRANSACTOR_NS_ALIVE PRAEF_METATRANSACTOR_NS_GRANT
#define PRAEF_METATRANSACTOR_NS_STILLBORN PRAEF_METATRANSACTOR_NS_DENY
#define PRAEF_METATRANSACTOR_NS_KILLED  \
  (PRAEF_METATRANSACTOR_NS_GRANT | PRAEF_METATRANSACTOR_NS_DENY)

/**
 * The metatransactor manages messages sent from particular nodes as well as
 * meta-events controlling nodes' stati.
 *
 * A metatransactor is typically layered directly on top of a transactor.
 * However, this is abstracted into the cxn interface for the sake of
 * modularity.
 */
typedef struct praef_metatransactor_s praef_metatransactor;
/**
 * Used by a metatransactor to talk to a lower-level component.
 */
typedef struct praef_metatransactor_cxn_s praef_metatransactor_cxn;

/**
 * Method on a metatransactor's connection informing it that an event destined
 * for the lower-level component has been accepted. The cxn MAY call the free
 * method on the event; doing so is guaranteed to have no effect, as the event
 * is managed by the metatransactor.
 */
typedef void (*praef_metatransactor_cxn_accept_t)(
  praef_metatransactor_cxn*, praef_event*);
/**
 * Method on a metatransactor's connection informing that an event which was
 * formerly passed into accept() is no longer considered to be present in the
 * system and is to be removed.
 */
typedef void (*praef_metatransactor_cxn_redact_t)(
  praef_metatransactor_cxn*, praef_event*);
/**
 * Method on a metatransactor's connection which is to create an event
 * describing a change in node count at a certain time.
 *
 * The returned event is owned by the metatransactor.
 *
 * In the case of connecting directly to a transactor, this corresponds to the
 * praef_transactor_node_count_delta() function.
 */
typedef praef_event* (*praef_metatransactor_cxn_node_count_delta_t)(
  praef_metatransactor_cxn*, signed, praef_instant);

struct praef_metatransactor_cxn_s {
  praef_metatransactor_cxn_accept_t accept;
  praef_metatransactor_cxn_redact_t redact;
  praef_metatransactor_cxn_node_count_delta_t node_count_delta;
};

/**
 * Creates a new metatransactor using the given connection. It is the caller's
 * responsibility to ensure that the connection object remains valid for the
 * lifetime of the metatransactor.
 *
 * @return The new metatransactor, or NULL if memory could not be
 * allocated. The metatransactor will already have the bootstrap node created
 * with a status of ALIVE.
 */
praef_metatransactor* praef_metatransactor_new(praef_metatransactor_cxn*);
/**
 * Frees the resources held by the given metatransactor.
 */
void praef_metatransactor_delete(praef_metatransactor*);

/**
 * Adds an event destined for the lower-level component to the given
 * metatransactor.
 *
 * @param node The node which produced this event. This MUST be an existing
 * node.
 * @param evt The event to be added. This object becomes owned by the
 * metatransactor regardles of whether the operation succeeds (ie, on failure,
 * it is freed immediately).
 * @return Whether the operation succeeds. Failure can occur due to memory
 * exhaustion, the given node not existing, or an event with the given
 * identifying triple already existing.
 */
int praef_metatransactor_add_event(praef_metatransactor*,
                                   praef_object_id node,
                                   praef_event* evt);
/**
 * Adds a node to the given metatransactor.
 *
 * @param node The ID of the new node to create, which will have status
 * UNBORN.
 * @return Whether the operation succeeds.
 */
int praef_metatransactor_add_node(praef_metatransactor*,
                                  praef_object_id node);
/**
 * Registers a vote to change the status of a node in the given
 * metatransactor. Note that a vote does not carry unless a simple majority of
 * ALIVE nodes cast the same vote.
 *
 * @param target The node which is to be altered. This MUST be an existing
 * node.
 * @param voter The node which is casting this vote. This MUST be an existing
 * node. The call has no effect if the voter has already voted for the change.
 * @param mask The bitmask to OR with the node's status. Exactly one bit must
 * be set in the mask.
 * @param when The time at which the modification is to take place.
 * @return Whether the operation succeeds. The operation can fail due to memory
 * exhaustion, if either the node or voter does not exist, or if an invalid
 * mask is specified.
 */
int praef_metatransactor_chmod(praef_metatransactor*,
                               praef_object_id target,
                               praef_object_id voter,
                               praef_metatransactor_node_status mask,
                               praef_instant when);

/**
 * Returns whether a vote with the given parameters (as per
 * praef_metatransactor_chmod()) have already been added to the
 * metatransactor.
 */
int praef_metatransactor_has_chmod(praef_metatransactor*,
                                   praef_object_id target,
                                   praef_object_id voter,
                                   praef_metatransactor_node_status mask,
                                   praef_instant when);

/**
 * Returns the instant at which the given node gained/will gain the GRANT bit,
 * though usually the only future time returned will be ~0.
 *
 * @param node The id of the node being queried. This MUST be an existing node.
 */
praef_instant praef_metatransactor_get_grant(praef_metatransactor*,
                                             praef_object_id node);
/**
 * Returns the instant at which the given node gained/will gain the DENY bit,
 * though usually the only future time returned will be ~0.
 *
 * @param node The id of the node being queried. This MUST be an existing node.
 */
praef_instant praef_metatransactor_get_deny(praef_metatransactor*,
                                            praef_object_id node);

/**
 * Advances the time of the given metatransactor.
 */
void praef_metatransactor_advance(praef_metatransactor*, unsigned);

#endif /* LIBPRAEFECTUS_METATRANSACTOR_H_ */
