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

#include <stdio.h>
#include <stdlib.h>

#include <libpraefectus/common.h>
#include <libpraefectus/clock.h>

#define NUM_CLOCKS 5
#define NUM_PARTITIONED 1
#define NUM_VISIBLE_FROM_PARTITION 3 /* including self */
#define STEP 10
#define OBSOLESCENCE 20
#define TOLERANCE (STEP*STEP*2)
#define TICK_PROB (RAND_MAX/4*3)
#define STEPS 512

/* This program is not a unit test, but rather simulates a group of 5 clocks
 * and outputs information about their behaviour to stdout in CSV format.
 *
 * The network is held in a non-transitive partition, where 1 node can only see
 * two others, which each can see the whole network.
 *
 * (No malicious nodes are simulated, since they are easily defeated by outlier
 * checks.)
 *
 * On each step, each clock has a 75% chance of ticking 10 steps, then all
 * clock sources are updated.
 */
int main(void) {
  praef_clock clocks[NUM_CLOCKS];
  /* sources[i][j] is source from clocks[i] into clocks[j] */
  praef_clock_source sources[NUM_CLOCKS][NUM_CLOCKS];
  unsigned i, j, step, sum, avg;

  for (i = 0; i < NUM_CLOCKS; ++i)
    praef_clock_init(&clocks[i], OBSOLESCENCE, TOLERANCE);

  for (i = 0; i < NUM_CLOCKS; ++i)
    for (j = 0; j < NUM_CLOCKS; ++j)
      if (i != j)
        praef_clock_source_init(&sources[i][j], &clocks[j]);

  /* Write CSV header */
  for (i = 0; i < NUM_CLOCKS; ++i)
    printf("Absolute %d,", i);
  printf(",");
  for (i = 0; i < NUM_CLOCKS; ++i)
    printf("Drift %d,", i);
  printf(",");
  for (i = 0; i < NUM_CLOCKS; ++i)
    printf("Offset %d,", i);
  printf("\n");

  for (step = 0; step < STEPS; ++step) {
    for (i = 0; i < NUM_CLOCKS; ++i)
      if (rand() < TICK_PROB)
        praef_clock_tick(&clocks[i], STEP, 1);

    sum = 0;
    for (i = 0; i < NUM_CLOCKS; ++i) {
      sum += clocks[i].monotime;

      for (j = 0; j < NUM_CLOCKS; ++j) {
        if (i == j) continue;
        /* If either i or j is under NUM_PARTITIONED, the other variable must
         * be below NUM_VISIBLE_FROM_PARTITION. If both are at or above
         * NUM_PARTITIONED, they can also see each other.
         */
        if ((i < NUM_PARTITIONED && j < NUM_VISIBLE_FROM_PARTITION) ||
            (j < NUM_PARTITIONED && i < NUM_VISIBLE_FROM_PARTITION) ||
            (i >= NUM_PARTITIONED && j >= NUM_PARTITIONED)) {
          praef_clock_source_sample(&sources[i][j], &clocks[j],
                                    clocks[i].monotime, 0);
        }
      }
    }

    avg = sum / NUM_CLOCKS;

    for (i = 0; i < NUM_CLOCKS; ++i)
      printf("%d,", clocks[i].monotime);
    printf(",");
    for (i = 0; i < NUM_CLOCKS; ++i)
      printf("%d,", clocks[i].drift);
    printf(",");
    for (i = 0; i < NUM_CLOCKS; ++i)
      printf("%d,", clocks[i].monotime - avg);
    printf("\n");
  }

  return 0;
}
