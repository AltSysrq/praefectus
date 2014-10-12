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

#include <libpraefectus/system.h>
#include <libpraefectus/virtual-bus.h>

#define NUM_NODES 5
#define MIN_LATENCY 2
#define MAX_LATENCY 5

static praef_virtual_network* vnet;
static praef_virtual_bus* bus[NUM_NODES];
static unsigned num_nodes_created = 0;

void nbodies_config_init(const char*const* argv, unsigned argc) {
  vnet = praef_virtual_network_new();
}

unsigned nbodies_config_step(void) {
  praef_virtual_network_advance(vnet, 1);

  return num_nodes_created? 0 : NUM_NODES;
}

static void link(praef_virtual_bus* a, praef_virtual_bus* b) {
  praef_virtual_network_link* link =
    praef_virtual_bus_link(a, b);
  link->firewall_grace_period = 200;
  link->reliability = 65000;
  link->duplicity = 512;
  link->base_latency = MIN_LATENCY;
  link->variable_latency = MAX_LATENCY - MIN_LATENCY;
}

praef_system* nbodies_config_create_system(praef_app* app) {
  unsigned ix = num_nodes_created++, i;
  bus[ix] = praef_virtual_network_create_node(vnet);

  for (i = 0; i < ix; ++i) {
    link(bus[i], bus[ix]);
    link(bus[ix], bus[i]);
  }

  praef_system* sys = praef_system_new(
    app, praef_virtual_bus_mb(bus[ix]),
    praef_virtual_bus_address(bus[ix]),
    MAX_LATENCY, praef_sp_lax,
    praef_siv_4only,
    praef_snl_local,
    512);

  if (ix)
    praef_system_connect(sys, praef_virtual_bus_address(bus[0]));
  else
    praef_system_bootstrap(sys);

  return sys;
}

unsigned nbodies_config_num_steps(void) {
  return 16384;
}

unsigned nbodies_config_optimism(void) {
  return 5;
}
