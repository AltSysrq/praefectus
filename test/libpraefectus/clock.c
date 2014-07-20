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

#include <libpraefectus/clock.h>

defsuite(libpraefectus_clock);

deftest(isolated_clock_advances_naturally) {
  praef_clock clock;

  praef_clock_init(&clock, 0, 0);

  ck_assert_int_eq(0, clock.ticks);
  ck_assert_int_eq(0, clock.systime);
  ck_assert_int_eq(0, clock.monotime);

  praef_clock_tick(&clock, 5, 1);

  ck_assert_int_eq(5, clock.ticks);
  ck_assert_int_eq(5, clock.systime);
  ck_assert_int_eq(5, clock.monotime);
}

deftest(syncs_forward_with_only_source_not_counting_self) {
  praef_clock clock;
  praef_clock_source source;

  praef_clock_init(&clock, 20, 0);
  /* Jump-start ticks so latency can be handled properly right away */
  clock.ticks = 1000;
  praef_clock_source_init(&source, &clock);
  praef_clock_source_sample(&source, &clock, 10, 5);
  praef_clock_tick(&clock, 5, 0);

  ck_assert_int_eq(12, clock.monotime);
  ck_assert_int_eq(20, clock.systime);
  ck_assert_int_eq(1005, clock.ticks);

  praef_clock_tick(&clock, 1, 0);

  ck_assert_int_eq(17, clock.monotime);
  ck_assert_int_eq(21, clock.systime);
  ck_assert_int_eq(1006, clock.ticks);

  praef_clock_tick(&clock, 1, 0);

  ck_assert_int_eq(20, clock.monotime);
  ck_assert_int_eq(22, clock.systime);
  ck_assert_int_eq(1007, clock.ticks);

  praef_clock_tick(&clock, 1, 0);

  ck_assert_int_eq(22, clock.monotime);
  ck_assert_int_eq(23, clock.systime);
  ck_assert_int_eq(1008, clock.ticks);

  praef_clock_tick(&clock, 1, 0);

  ck_assert_int_eq(23, clock.monotime);
  ck_assert_int_eq(24, clock.systime);
  ck_assert_int_eq(1009, clock.ticks);
}

deftest(syncs_forward_with_only_other_source_counting_self) {
  praef_clock clock;
  praef_clock_source source;

  praef_clock_init(&clock, 20, 0);
  praef_clock_source_init(&source, &clock);
  praef_clock_source_sample(&source, &clock, 100, 0);

  praef_clock_tick(&clock, 1, 1);

  ck_assert_int_eq(26, clock.monotime);
  ck_assert_int_eq(51, clock.systime);
  ck_assert_int_eq(1, clock.ticks);

  praef_clock_tick(&clock, 1, 1);

  ck_assert_int_eq(52, clock.monotime);
  ck_assert_int_eq(77, clock.systime);
  ck_assert_int_eq(2, clock.ticks);
}

deftest(syncs_backward_with_only_source_not_counting_self) {
  praef_clock clock;
  praef_clock_source source;

  praef_clock_init(&clock, 200, 0);
  praef_clock_tick(&clock, 100, 1);

  ck_assert_int_eq(100, clock.monotime);
  ck_assert_int_eq(100, clock.systime);
  ck_assert_int_eq(100, clock.ticks);

  praef_clock_source_init(&source, &clock);
  praef_clock_source_sample(&source, &clock, 50, 0);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(105, clock.monotime);
  ck_assert_int_eq(60, clock.systime);
  ck_assert_int_eq(110, clock.ticks);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(110, clock.monotime);
  ck_assert_int_eq(70, clock.systime);
  ck_assert_int_eq(120, clock.ticks);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(115, clock.monotime);
  ck_assert_int_eq(80, clock.systime);
  ck_assert_int_eq(130, clock.ticks);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(120, clock.monotime);
  ck_assert_int_eq(90, clock.systime);
  ck_assert_int_eq(140, clock.ticks);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(125, clock.monotime);
  ck_assert_int_eq(100, clock.systime);
  ck_assert_int_eq(150, clock.ticks);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(130, clock.monotime);
  ck_assert_int_eq(110, clock.systime);
  ck_assert_int_eq(160, clock.ticks);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(135, clock.monotime);
  ck_assert_int_eq(120, clock.systime);
  ck_assert_int_eq(170, clock.ticks);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(140, clock.monotime);
  ck_assert_int_eq(130, clock.systime);
  ck_assert_int_eq(180, clock.ticks);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(145, clock.monotime);
  ck_assert_int_eq(140, clock.systime);
  ck_assert_int_eq(190, clock.ticks);

  praef_clock_tick(&clock, 10, 0);
  ck_assert_int_eq(150, clock.monotime);
  ck_assert_int_eq(150, clock.systime);
  ck_assert_int_eq(200, clock.ticks);
}

deftest(drift_is_debounced) {
  praef_clock clock;
  praef_clock_source source;

  praef_clock_init(&clock, 20, 10);
  praef_clock_source_init(&source, &clock);
  praef_clock_tick(&clock, 1, 1);
  praef_clock_source_sample(&source, &clock, 1, 0);

  praef_clock_tick(&clock, 4, 1);
  ck_assert_int_eq(5, clock.monotime);
  ck_assert_int_eq(5, clock.systime);
  ck_assert_int_eq(5, clock.ticks);

  /* Other source drifts ahead by 4 */
  praef_clock_source_sample(&source, &clock, 9, 0);

  /* First couple ticks have no effect due to debounce */
  praef_clock_tick(&clock, 1, 1);
  ck_assert_int_eq(6, clock.monotime);
  ck_assert_int_eq(8, clock.systime);
  ck_assert_int_eq(6, clock.ticks);

  praef_clock_tick(&clock, 1, 1);
  ck_assert_int_eq(7, clock.monotime);
  ck_assert_int_eq(10, clock.systime);
  ck_assert_int_eq(7, clock.ticks);

  /* After more than 5 ticks, drift passes tolerance and the clock is partially
   * resynced.
   */
  praef_clock_tick(&clock, 4, 1);
  ck_assert_int_eq(12, clock.monotime);
  ck_assert_int_eq(14, clock.systime);
  ck_assert_int_eq(11, clock.ticks);
}

deftest(sources_with_zero_time_are_ignored) {
  praef_clock clock;
  praef_clock_source source;

  praef_clock_init(&clock, 0, 0);
  praef_clock_source_init(&source, &clock);

  praef_clock_tick(&clock, 5, 1);
  praef_clock_tick(&clock, 5, 1);
  ck_assert_int_eq(10, clock.monotime);
  ck_assert_int_eq(10, clock.systime);
  ck_assert_int_eq(10, clock.ticks);
}

deftest(sources_with_obsolete_reports_are_ignored) {
  praef_clock clock;
  praef_clock_source source;

  praef_clock_init(&clock, 5, 0);
  praef_clock_source_init(&source, &clock);

  praef_clock_tick(&clock, 10, 1);
  praef_clock_source_sample(&source, &clock, 1, 6);
  praef_clock_tick(&clock, 1, 1);

  ck_assert_int_eq(11, clock.monotime);
  ck_assert_int_eq(11, clock.systime);
  ck_assert_int_eq(11, clock.ticks);
}

deftest(outliers_are_excluded) {
  praef_clock clock;
  praef_clock_source out_low, out_high, coop;

  praef_clock_init(&clock, 20, 0);
  praef_clock_source_init(&out_low, &clock);
  praef_clock_source_init(&out_high, &clock);
  praef_clock_source_init(&coop, &clock);

  praef_clock_tick(&clock, 10, 1);
  praef_clock_source_sample(&out_low, &clock, 1, 0);
  praef_clock_source_sample(&out_high, &clock, 1024, 0);
  praef_clock_source_sample(&coop, &clock, 20, 0);
  praef_clock_tick(&clock, 1, 1);

  ck_assert_int_eq(13, clock.monotime);
  ck_assert_int_eq(16, clock.systime);
  ck_assert_int_eq(11, clock.ticks);
}
