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
#ifndef LIBPRAEFECTUS_OBJECT_H_
#define LIBPRAEFECTUS_OBJECT_H_

#include "common.h"

/**
 * Globally identifies a praefectus object within a single context. Object ids
 * are to be provided by the application, except that object ID 0 is reserved
 * for internal use.
 */
typedef unsigned praef_object_id;

/**
 * The reserved null object id; application objects may not have this ID.
 */
#define PRAEF_NULL_OBJECT_ID ((praef_object_id)0)

typedef struct praef_object_s praef_object;
/**
 * Function type for the step method on a praefectus object. The given object
 * is to be advanced in time by exactly one instant.
 */
typedef void (*praef_object_step_t)(praef_object*, praef_userdata);
/**
 * Function type for the rewind method on a praefectus object. The state of the
 * given object is to be reverted to the state it had at the beginning of the
 * specified instant (that is, its state in that instant before any events were
 * applied).
 */
typedef void (*praef_object_rewind_t)(praef_object*, praef_instant);

/**
 * The base information for an "object" in praefectus. A praefectus object is
 * generally much larger in scope than an object in a traditional sense; for
 * example, in the demonstration game, each object represents the full state of
 * a player or the environment, including all physical entities associated
 * therewith.
 *
 * There are two primitive operations objects must implement. The first is the
 * step operation, which advances time for that object by one instant. Multiple
 * objects within one context are stepped once per instant, in ascending order
 * by id. The second is the rewind operation, which resets the object to an
 * earlier state.
 *
 * The amount of history an object must maintain is entirely dependent on the
 * application --- objects are rewound in response to event changes that edit
 * history.
 */
struct praef_object_s {
  /**
   * Specifies this object's implementation of the step method.
   */
  praef_object_step_t step;
  /**
   * Specifies this object's implementation of the rewind method.
   */
  praef_object_rewind_t rewind;

  /**
   * The context-unique identifier for this object.
   */
  praef_object_id id;

  /**
   * Entry for an RB-Tree which can be used to look objects up by id, and
   * additionally maintains them sorted by id.
   */
  RB_ENTRY(praef_object_s) idmap;
};

/**
 * Compares to praefectus objects by their id, as per tree(3).
 */
int praef_compare_object_id(const praef_object*, const praef_object*);

RB_PROTOTYPE(praef_object_idmap, praef_object_s, idmap,
             praef_compare_object_id);

#endif /* LIBPRAEFECTUS_OBJECT_H_ */
