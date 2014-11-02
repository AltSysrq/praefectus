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
#ifndef LIBPRAEFECTUS_STD_STATE_H_
#define LIBPRAEFECTUS_STD_STATE_H_

#include "context.h"
#include "transactor.h"
#include "metatransactor.h"
#include "mtt-bridge.h"

__BEGIN_DECLS

/**
 * The std_state structure is a convenience for managing the "standard" stack
 * of validated, deserialised state, consisting of a context, transactor, and
 * metatransactor, always kept in-sync with each other.
 */
typedef struct {
  praef_context* context;
  praef_transactor* tx;
  praef_metatransactor* mtx;
  praef_mtt_bridge bridge;
} praef_std_state;

/**
 * Initialises the given std_state, filling the structure in with new values.
 *
 * If this call fails, the contents of the std_state are undefined, but no
 * memory will have been leaked, and praef_std_state_cleanup() is not to be
 * called.
 *
 * @return Whether the operation succeeds.
 */
int praef_std_state_init(praef_std_state*);
/**
 * Cleans up the resources held by the given std_state. Does not free the
 * std_state itself.
 */
void praef_std_state_cleanup(praef_std_state*);

/**
 * Properly advances the metatransactor, transactor, and context by the given
 * amount, passing the given userdata to the advancement of the context.
 *
 * Using a delta of zero is meaningful, and simply brings the whole system into
 * a consistent state.
 */
void praef_std_state_advance(praef_std_state*, unsigned, praef_userdata);

__END_DECLS

#endif /* LIBPRAEFECTUS_STD_STATE_H_ */
