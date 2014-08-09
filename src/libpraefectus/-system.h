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
#ifndef LIBPRAEFECTUS__SYSTEM_H_
#define LIBPRAEFECTUS__SYSTEM_H_

/* This is an internal header file.
 *
 * It defines the internal data structures for praef_system and related, so
 * that tests may directly access them.
 *
 * Functions declared by this file are implemented in system-core.c. Other
 * implementation files have their own internal headers.
 */

#include "clock.h"
#include "commitment-chain.h"
#include "dsa.h"
#include "hash-tree.h"
#include "system.h"

typedef struct praef_extnode_s praef_extnode;

RB_HEAD(praef_extnode_map,praef_extnode_s);

struct praef_system_s {
  praef_app* app;
  praef_message_bus* bus;
  PraefNetworkIdentifierPair_t net_id;

  struct praef_extnode_map nodes;
  /**
   * The id of the local node, or 0 if unknown.
   */
  praef_object_id self_id;

  praef_clock clock;
  praef_hash_tree* hash_tree;

  praef_signator* signator;
  praef_verifier* verifier;
};

struct praef_extnode_s {
  praef_object_id id;
  praef_system* system;
  PraefNetworkIdentifierPair_t net_id;

  praef_clock_source clock_source;
  praef_comchain* comchain;

  RB_ENTRY(praef_extnode_s) map;
};

/**
 * Compares two extnodes by id, as per tree(3). This only looks at the id, so
 * passing in a praef_object_id* instead of an actual praef_extnode* is
 * safe. (This makes doing lookups in the tree much more economical.)
 */
int praef_compare_extnode(const praef_extnode*, const praef_extnode*);

RB_PROTOTYPE(praef_extnode_map, praef_extnode_s, map, praef_compare_extnode)

praef_extnode* praef_extnode_new(praef_system*, praef_object_id,
                                 const PraefNetworkIdentifierPair_t*);
void praef_extnode_delete(praef_extnode*);

#endif /* LIBPRAEFECTUS__SYSTEM_H_ */
