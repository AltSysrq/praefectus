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
#ifndef LIBPRAEFECTUS_COMMITMENT_CHAIN_H_
#define LIBPRAEFECTUS_COMMITMENT_CHAIN_H_

#include "common.h"

/**
 * A comchain (commitment chain) is used to track commitments and reveals from
 * other nodes, in order to determine what data they have access to. It is also
 * used for the production of such data.
 */
typedef struct praef_comchain_s praef_comchain;

/**
 * Allocates a new, empty comchain. Returns NULL if the operation fails.
 */
praef_comchain* praef_comchain_new(void);
/**
 * Frees the memory held by the given comchain.
 */
void praef_comchain_delete(praef_comchain*);

/**
 * Adds a commit to the given comchain. A commit specifies a time range and the
 * expected second-order hash of the objects that occur within that range
 * (irrespective of their order).
 *
 * Commits may not overlap; introducing an overlapping commit puts the comchain
 * into a permanent error state. Holes are permitted between commits, and
 * commits may be introduced in any order, but they must eventually be
 * contiguous if one wishes the commit and reveal thresholds to move forward.
 *
 * @param start_inclusive The instant, inclusive, at which this commit begins.
 * @param end_exclusive The instant, exclusive, at which this commit ends.
 * @param hash The second-order hash (ie, hash of hashes) to expect for this
 * time interval.
 * @return Whether the operation succeeds. Note that an operation that kills
 * the comchain is still considered success, as are operations ignored due to a
 * dead comchain.
 */
int praef_comchain_commit(praef_comchain*,
                          praef_instant start_inclusive,
                          praef_instant end_exclusive,
                          const unsigned char hash[PRAEF_HASH_SIZE]);
/**
 * Reveals the hash of an object occurring at a particular time. If a commit
 * already covers the given instant, the hash is added to the
 * commit. Otherwise, the information is held until such a time that there is a
 * commit for it.
 *
 * Note that it is possible for this call to move a commit *out of* a valid
 * state. Such a transition is permanent, since reversing it would require
 * breaking SHA-3.
 *
 * There is no guarantee, however, that such post-facto invalidation will
 * actually occur, even if a set of objects are revealed that makes satisfying
 * the hash impossible. This condition is only detected if, at some point in
 * time, the *exact* set of objects to satisfy the hash is present, and then
 * later objects are added. For example, if a hash would be satisfied with
 * objects A and B, adding A, B, and C or B, A, and C (in one of those two
 * orders) will invalidate the commit. However, if the objects are added in any
 * other order (eg, A, C, B), the commit will languish in the pending state,
 * even though it is impossible for it ever to become valid. This will also
 * happen if an object which would hypothetically invalidate the commit is
 * added before the commit itself.
 *
 * It is an error to reveal the same object multiple times.
 *
 * @return Whether the operation succeeds. Note that an operation that kills
 * the comchain is still considered success, as are operations ignored due to a
 * dead comchain.
 */
int praef_comchain_reveal(praef_comchain*,
                          praef_instant,
                          const unsigned char hash[PRAEF_HASH_SIZE]);

/**
 * Returns the instant of the "committed threshold" for this comchain. The
 * committed threshold corresponds to the end instant of the last commit in the
 * sequence of contiguous commits wherein the first commit's start time is
 * zero. If there is no such sequence, 0 is returned. In certain permanent
 * error conditions, 0 may be returned instead of a real value.
 */
praef_instant praef_comchain_committed(const praef_comchain*);
/**
 * Returns the instant of the "validated threshold" for this comchain. The
 * validated threshold corresponds to the end instant of the last commit in the
 * sequence of contiguous commits whose expected second-order hash has been
 * attained, wherein the first commit's start time is zero. If there is no such
 * sequence, zero is returned.
 *
 * The amount by which the validated threshold rolls backward upon the
 * invalidation of a previously-validated commit via praef_comchain_reveal() is
 * undefined.
 */
praef_instant praef_comchain_validated(const praef_comchain*);
/**
 * Returns whether any permanent error conditions have been detected within
 * this comchain. Once this begins returning true, it will never return false,
 * and neither threshold will advance any further.
 */
int praef_comchain_is_dead(const praef_comchain*);

/**
 * Creates and adds a commit for the given time interval, calculating the
 * appropriate hash required for that commit to be valid. This hash is returned
 * by filling in the hash field.
 *
 * If the comchain has entered a state of permanent invalidity (see
 * praef_comchain_is_dead()), this call always fails. Calls that cause the
 * comchain to become invalid are also considered failures, since in this case
 * no hash value is available, and it almost certainly indicates a bug in the
 * caller.
 *
 * @return Whether the operation succeeds.
 */
int praef_comchain_create_commit(unsigned char hash[PRAEF_HASH_SIZE],
                                 praef_comchain*,
                                 praef_instant start_inclusive,
                                 praef_instant end_exclusive);

#endif /* LIBPRAEFECTUS_COMMITMENT_CHAIN_H_ */
