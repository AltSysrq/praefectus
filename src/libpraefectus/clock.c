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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "clock.h"

void praef_clock_init(praef_clock* clock, unsigned obsolescence_interval,
                      unsigned tolerance) {
  clock->monotime = clock->systime = clock->ticks = 0;
  clock->drift = 0;
  clock->obsolescence_interval = obsolescence_interval;
  clock->tolerance = tolerance;
  SLIST_INIT(&clock->sources);
}

void praef_clock_source_init(praef_clock_source* source, praef_clock* clock) {
  source->latest = 0;
  source->last_update = 0;
  SLIST_INSERT_HEAD(&clock->sources, source, next);
}

void praef_clock_source_destroy(praef_clock_source* source,
                                praef_clock* clock) {
  SLIST_REMOVE(&clock->sources, source, praef_clock_source_s, next);
}

void praef_clock_source_sample(praef_clock_source* source,
                               const praef_clock* clock,
                               praef_instant instant,
                               unsigned latency) {
  if (instant >= source->latest) {
    source->latest = instant;
    if (latency <= clock->ticks)
      source->last_update = clock->ticks - latency;
    else
      source->last_update = 0;
  }
}

static unsigned praef_clock_num_sources(praef_clock* clock) {
  unsigned count = 0;
  praef_clock_source* source;

  SLIST_FOREACH(source, &clock->sources, next)
    ++count;

  return count;
}

static int compare_instants(const void* pa, const void* pb) {
  praef_instant a = *(const praef_instant*)pa;
  praef_instant b = *(const praef_instant*)pb;

  return ((a > b) - (a < b));
}

void praef_clock_tick(praef_clock* clock, unsigned delta, int count_self) {
  praef_clock_source* source;
  praef_instant samples[1 + praef_clock_num_sources(clock)];
  praef_instant* filtered_samples;
  praef_instant natural_monotime;
  unsigned long long sum;
  unsigned num_reporting, i;

  clock->ticks += delta;
  clock->systime += delta;

  /* Collect the samples from each non-obsoleted source reporting non-zero into
   * the samples array, extrapolating from the last repor.
   */
  num_reporting = 0;
  SLIST_FOREACH(source, &clock->sources, next)
    if (0 != source->latest &&
        source->last_update + clock->obsolescence_interval > clock->ticks)
      samples[num_reporting++] = source->latest +
        (clock->ticks - source->last_update);

  if (0 != clock->systime && count_self)
    samples[num_reporting++] = clock->systime;

  /* Throw some samples out if there are enough samples. There are fewer than 2
   * samples, we can't really do anything meaningful but average them, so leave
   * everything in in that case.
   */
  if (num_reporting < 3) {
    filtered_samples = samples;
  } else {
    /* Sort the samples ascending, then chop off the upper and lower 25% of
     * samples to resist deliberate sabotage and severely out-of-sync nodes.
     */
    qsort(samples, num_reporting, sizeof(praef_instant), compare_instants);
    /* +1 below so that we still drop samples with 3 nodes (in particular, 3
     * nodes will always just take the median instead of averaging anything).
     */
    filtered_samples = samples + (num_reporting+1) / 4;
    num_reporting -= (num_reporting+1) / 4 * 2;
  }

  /* Average is meaningless if there are no samples. In such a case, just leave
   * the systime alone for now.
   */
  if (num_reporting > 0) {
    sum = 0;
    for (i = 0; i < num_reporting; ++i)
      sum += filtered_samples[i];

    clock->systime = sum / num_reporting;
  }

  natural_monotime = clock->monotime + delta;
  clock->drift += (signed)delta * (signed)(natural_monotime - clock->systime);

  if (((unsigned)abs(clock->drift)) < clock->tolerance) {
    /* Drift still within tolerable range, don't adjust */
    clock->monotime = natural_monotime;
  } else {
    /* Drift beyond tolerance. Advance monotime at half speed and cut the drift
     * accumulator in half. (The latter reduces the effect of bouncing even
     * more when the drift is small, but ensures that very large drifts are
     * rectified more quickly.)
     */
    if (delta > 1)
      clock->monotime += delta/2;
    else if (1 == delta && 1 == (clock->ticks & 1))
      /* delta/2 == 0, so instead advance by 1 every other tick */
      ++clock->monotime;

    clock->drift /= 2;

    /* If the result is behind systime, move to the average of natural_monotime
     * and systime if natural_monotime < systime, and snap to systime
     * otherwise. If snapping, zero drift; otherwise, half it (again).
     */
    if (clock->monotime <= clock->systime) {
      if (natural_monotime < clock->systime) {
        sum = 0;
        sum += natural_monotime;
        sum += clock->systime;
        clock->monotime = sum/2;
        clock->drift /= 2;
      } else {
        clock->monotime = clock->systime;
        clock->drift = 0;
      }
    }
  }
}
