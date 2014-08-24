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
#ifndef LIBPRAEFECTUS_STDSYS_H_
#define LIBPRAEFECTUS_STDSYS_H_

#include "system.h"
#include "std-state.h"

/**
 * Creates a partial application interface which implements all the *_bridge()
 * methods via calls to the given standard state stack.
 *
 * All non-bridge (both optional and mandatory) methods in the returned
 * application interface are NULL. The application MUST still provide its own
 * implementations for the mandatory not-bridge methods. The application MAY
 * overwrite any number of *_bridge() methods if it wishes to do so.
 *
 * Note that the implementation of praef_app::neutralise_event_bridge() works
 * by overwriting the praef_event::apply() method of events with a no-op
 * function and a forced rewind of the underlying context to the instant of the
 * event. This is unsuitable for applications which use data in the events to
 * perform roll-back; such applications MUST provide their own implementation
 * of this method which neutralises both the apply and roll-back behaviours.
 *
 * After the praef_system is constructed with this praef_app,
 * praef_stdsys_system() MUST be called to inform the bridge of the system in
 * which it is participating.
 *
 * @param stack The initialised standard-state stack. The application MUST
 * ensure this remains valid for the lifetime of the bridge.
 * @return An application interface bridging to stack, or NULL if memory
 * allocation fails.
 */
praef_app* praef_stdsys_new(praef_std_state* stack);
/**
 * Sets the praef_system associated with the given standard system bridge.
 *
 * The given praef_app MUST be an object that was returned from
 * praef_stdsys_new().
 */
void praef_stdsys_system(praef_app*, praef_system*);
/**
 * Configures the userdata passed into praef_context_advance() (and, by proxy,
 * to the advance and apply methods of application objects and events,
 * respectively).
 *
 * If this is not called, NULL userdata will be passed in.
 *
 * The given praef_app MUST be an object that was returned from
 * praef_stdsys_new().
 */
void praef_stdsys_userdata(praef_app*, praef_userdata);
/**
 * Configures how optimistic events are classified. The given callback will be
 * invoked for each event to be added to the underlying context. If it returns
 * non-zero, the event will be considered optimistic with a deadline that many
 * instants after the event's instant. Returning 0 indicates a pessimistic
 * event.
 *
 * If this is never called, all events will be considered pessimistic.
 *
 * The given praef_app MUST be an object that was returned from
 * praef_stdsys_new().
 */
void praef_stdsys_optimistic_events(praef_app*,
                                    unsigned (*)(const praef_event*));
/**
 * Frees the given standard system interface, which MUST be an object returned
 * from praef_stdsys_new().
 */
void praef_stdsys_delete(praef_app*);

#endif /* LIBPRAEFECTUS_STDSYS_H_ */
