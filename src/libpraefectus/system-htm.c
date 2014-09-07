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

#include "system.h"
#include "-system.h"

int praef_system_htm_init(praef_system* sys) {
  if (!praef_system_conf_ht_num_snapshots(sys, 64)) return 0;

  sys->htm.range_max = 64;
  sys->htm.range_query_interval = sys->std_latency*4;
  sys->htm.snapshot_interval = sys->std_latency;
  sys->htm.root_query_interval = sys->std_latency*16;
  sys->htm.root_query_offset = sys->std_latency*4;
  return 1;
}

void praef_system_htm_destroy(praef_system* sys) {
  unsigned i;

  if (sys->htm.snapshots) {
    for (i = 0; i < sys->htm.num_snapshots; ++i)
      if (sys->htm.snapshots[i].tree)
        praef_hash_tree_delete(sys->htm.snapshots[i].tree);

    free(sys->htm.snapshots);
  }
}

int praef_node_htm_init(praef_node* node) {
  return 1;
}

void praef_node_htm_destroy(praef_node* node) { }

void praef_system_conf_ht_range_max(praef_system* sys, unsigned max) {
  sys->htm.range_max = max;
}

void praef_system_conf_ht_range_query_interval(praef_system* sys,
                                               unsigned interval) {
  sys->htm.range_query_interval = interval;
}

void praef_system_conf_ht_snapshot_interval(praef_system* sys,
                                            unsigned interval) {
  sys->htm.snapshot_interval = interval;
}

int praef_system_conf_ht_num_snapshots(praef_system* sys,
                                       unsigned count) {
  praef_system_htm_snapshot* snapshots;
  unsigned i;

  snapshots = calloc(count, sizeof(praef_system_htm_snapshot));
  if (!snapshots) return 0;

  if (sys->htm.snapshots) {
    for (i = 0; i < sys->htm.num_snapshots; ++i)
      if (sys->htm.snapshots[i].tree)
        praef_hash_tree_delete(sys->htm.snapshots[i].tree);
    free(sys->htm.snapshots);
  }

  sys->htm.snapshots = snapshots;
  sys->htm.num_snapshots = count;
  return 1;
}

void praef_system_conf_ht_root_query_interval(praef_system* sys,
                                              unsigned interval) {
  sys->htm.root_query_interval = interval;
}

void praef_system_conf_ht_root_query_offset(praef_system* sys,
                                            unsigned offset) {
  sys->htm.root_query_offset = offset;
}
