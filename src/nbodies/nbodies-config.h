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
#ifndef NBODIES_NBODIES_CONFIG_H_
#define NBODIES_NBODIES_CONFIG_H_

#include <libpraefectus/system.h>

/**
 * @file
 *
 * Defines the interface between the nbodies implementation core and the
 * "configuration" implementations to which it can be linked.
 */

/**
 * The fixed interval at which events are produced.
 */
#define EVENT_INTERVAL 10

/**
 * Initialises the configuration.
 *
 * @param argv The raw command-line arguments given to the process.
 * @param argc The length of argv.
 */
void nbodies_config_init(const char*const* argv, unsigned argc);
/**
 * Steps the configuration's plan ahead by one step.
 * nbodies_config_create_system() will be called the number of times indicated
 * by the return value of this function.
 *
 * If the configuration needs to slow the simulation down to real time, it may
 * do so during this call.
 *
 * @return The number of new systems to create this instant.
 */
unsigned nbodies_config_step(void);
/**
 * Creates and bootstraps/connects the next system in the simulation.
 *
 * @param praef_app* The application interface for the new system.
 * @return The new system.
 */
praef_system* nbodies_config_create_system(praef_app*);

/**
 * Returns the total number of steps to take.
 */
unsigned nbodies_config_num_steps(void);

/**
 * Returns the event optimism to use.
 */
unsigned nbodies_config_optimism(void);

#endif /* NBODIES_NBODIES_CONFIG_H_ */
