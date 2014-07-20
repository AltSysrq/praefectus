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
#ifndef LIBPRAEFECTUS_CLOCK_H_
#define LIBPRAEFECTUS_CLOCK_H_

#include "common.h"

/**
 * A clock source is a reference to what some external source believes the
 * current time is. Every clock source is registered with exactly one clock,
 * which uses the sources to try to synchronise with the sources.
 *
 * Clock sources are initialised with praef_clock_source_init() and destroyed
 * by praef_clock_source_destroy(). Their destruction is unnecessary (and has
 * undefined behaviour) if the clock is destroyed first. Clock sources are
 * updated with praef_clock_source_sample().
 */
typedef struct praef_clock_source_s praef_clock_source;
struct praef_clock_source_s {
  /**
   * The greatest time ever reported from this source. A value of zero
   * indicates that this source either does not know the current time or has
   * not yet reported it.
   */
  praef_instant latest;
  /**
   * The last time (internal to the clock) at which this source was updated,
   * taking estimated latency into account.
   */
  unsigned last_update;

  SLIST_ENTRY(praef_clock_source_s) next;
};

/**
 * The clock structure is used to synchronise the time of the system across all
 * nodes, taking estimated latency into account. It is resistant to sabotage
 * from other nodes.
 *
 * The clock does assume that the clock of the underlying platform is
 * reasonably accurate over a certain obsolescence interval. It is assumed it
 * is safe to use the local time-source to extrapolate sources' current times
 * from their last report until their last report is more than that interval in
 * the past.
 *
 * Clocks are initialised with praef_clock_init(), and are destroyed simply by
 * releasing whatever memory held them.
 */
typedef struct {
  /**
   * The current monotonically-increasing time. This is guaranteed to never be
   * reduced for the life of the clock; if it needs to be adjusted backwards,
   * its advancement is slowed until it syncs with the systime.
   *
   * This is the concept of time that should be used for most purposes.
   */
  praef_instant monotime;
  /**
   * The current time of the full praefectus system. There are no guarantees of
   * how this value behaves.
   */
  praef_instant systime;
  /**
   * The number of time-steps this clock has been advanced. This is local to
   * the clock, and is used for determining durations and such, since it moves
   * forward at a constant rate exactly as determined by the platform clock.
   */
  unsigned ticks;

  /**
   * The number of ticks since the last report after which a clock source is no
   * longer considered relevant.
   */
  unsigned obsolescence_interval;
  /**
   * The maximum absolute value of the drift field before monotime ceases to
   * progress naturally.
   */
  unsigned tolerance;

  /**
   * The integral of the difference between monotime and systime, accumulated
   * each tick. This is used to debounce momentary variations in systime to
   * keep a smooth advancement of monotime when possible.
   */
  signed drift;

  SLIST_HEAD(,praef_clock_source_s) sources;
} praef_clock;

/**
 * Initialises all the fields of the given clock. All times are initialised to
 * zero, and the clock has no sources.
 *
 * @param obsolescence_interval The value for the obsolescence_interval field
 * of the praef_clock struct.
 */
void praef_clock_init(praef_clock*, unsigned obsolescence_interval,
                      unsigned tolerance);
/**
 * Initialises the given clock source and adds it to the given clock. The data
 * fields of the source are both set to zero.
 */
void praef_clock_source_init(praef_clock_source*, praef_clock*);
/**
 * Removes the given clock source from the given clock. The source MUST be
 * present within the clock.
 */
void praef_clock_source_destroy(praef_clock_source*, praef_clock*);

/**
 * Updates the sampling for a clock source.
 *
 * @param instant The time reported by the source.
 * @param latency The estimated latency (in ticks) of the source's report.
 */
void praef_clock_source_sample(praef_clock_source*, const praef_clock*,
                               praef_instant instant, unsigned latency);
/**
 * Advances the clock the given number of ticks and updates the estimated
 * system and mono times.
 *
 * @param delta The number of ticks to advance.
 * @param count_self Whether to count the clock's on monotime as a source.
 */
void praef_clock_tick(praef_clock*, unsigned delta, int count_self);

#endif /* LIBPRAEFECTUS_CLOCK_H_ */
