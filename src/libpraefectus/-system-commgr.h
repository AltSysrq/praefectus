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
#ifndef LIBPRAEFECTUS__SYSTEM_COMMGR_H_
#define LIBPRAEFECTUS__SYSTEM_COMMGR_H_

/* This is an internal header file.
 *
 * The commgr manages all commit concerns, including generating commit messages
 * and maintaining the visibility horizon for each node.
 */

#include "messages/PraefMsgCommit.h"
#include "system.h"
#include "-system.h"

/**
 * Initialises the portion of the praef_system managed by the commitment
 * manager.
 *
 * @return Whether the operation succeeds.
 */
int praef_system_commgr_init(praef_system*,
                             unsigned std_latency,
                             praef_system_profile);
/**
 * Destroys the portion of the praef_system managed by the commitment manager.
 */
void praef_system_commgr_destroy(praef_system*);

/**
 * Updates the global state managed by the commitment manager. In particular,
 * this produces commit messages. This needs to be called after
 * committed-redistributable messages have been flushed.
 */
void praef_system_commgr_update(praef_system*);

/**
 * Updates the commit-related state of the given extnode.
 */
void praef_extnode_commgr_update(praef_extnode*);
/**
 * Notifies the commitment manager of a committed-redistributable messages that
 * has been received from the given node.
 */
void praef_extnode_commgr_receive(praef_extnode*, const praef_hlmsg*);
/**
 * Notifies the commitment manager of a commit message received.
 */
void praef_extnode_commgr_commit(praef_extnode*,
                                 praef_instant, const PraefMsgCommit_t*);

/**
 * Calculates the visibility horizon for the given extnode, based upon its
 * commit threshold. Committed-redistributable messages whose timestamp is
 * greater than this value shall not be sent to this node.
 */
praef_instant praef_extnode_commgr_horizon(praef_extnode*);

#endif /* LIBPRAEFECTUS__SYSTEM_COMMGR_H_ */
