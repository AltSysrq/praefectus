/*-
 * Copyright (c)  2014 Jason Lingle
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

#include <libpraefectus/commitment-chain.h>

defsuite(libpraefectus_commitment_chain);

static praef_comchain* chains[3];
static unsigned char hashes[32][PRAEF_HASH_SIZE];

defsetup {
  unsigned i, j;

  for (i = 0; i < 3; ++i)
    chains[i] = praef_comchain_new();

  for (i = 0; i < 32; ++i)
    for (j = 0; j < PRAEF_HASH_SIZE; ++j)
      hashes[i][j] = rand();
}

defteardown {
  unsigned i;

  for (i = 0; i < 3; ++i)
    praef_comchain_delete(chains[i]);
}

deftest(linear_commit_threshold_advances) {
  ck_assert_int_eq(0, praef_comchain_committed(chains[0]));
  praef_comchain_commit(chains[0], 0, 3, hashes[0]);
  ck_assert_int_eq(3, praef_comchain_committed(chains[0]));
  praef_comchain_commit(chains[0], 3, 5, hashes[1]);
  ck_assert_int_eq(5, praef_comchain_committed(chains[0]));
}

deftest(nonlinear_commit_threshold_advances_when_made_contiguous) {
  ck_assert_int_eq(0, praef_comchain_committed(chains[0]));
  praef_comchain_commit(chains[0], 3, 5, hashes[0]);
  ck_assert_int_eq(0, praef_comchain_committed(chains[0]));
  praef_comchain_commit(chains[0], 7, 8, hashes[0]);
  ck_assert_int_eq(0, praef_comchain_committed(chains[0]));
  praef_comchain_commit(chains[0], 0, 3, hashes[0]);
  ck_assert_int_eq(5, praef_comchain_committed(chains[0]));
  praef_comchain_commit(chains[0], 5, 7, hashes[0]);
  ck_assert_int_eq(8, praef_comchain_committed(chains[0]));
}

deftest(simultaneous_commits_invalidate_chain) {
  ck_assert(!praef_comchain_is_dead(chains[0]));
  ck_assert(praef_comchain_commit(chains[0], 2, 3, hashes[0]));
  ck_assert(!praef_comchain_is_dead(chains[0]));
  ck_assert(praef_comchain_commit(chains[0], 2, 4, hashes[0]));
  ck_assert(praef_comchain_is_dead(chains[0]));
}

deftest(past_existing_overlapping_commit_invalidates_chain) {
  ck_assert(!praef_comchain_is_dead(chains[0]));
  ck_assert(praef_comchain_commit(chains[0], 2, 10, hashes[0]));
  ck_assert(!praef_comchain_is_dead(chains[0]));
  ck_assert(praef_comchain_commit(chains[0], 5, 15, hashes[0]));
  ck_assert(praef_comchain_is_dead(chains[0]));
}

deftest(future_existing_overlapping_commit_invalidates_chain) {
  ck_assert(!praef_comchain_is_dead(chains[0]));
  ck_assert(praef_comchain_commit(chains[0], 5, 15, hashes[0]));
  ck_assert(!praef_comchain_is_dead(chains[0]));
  ck_assert(praef_comchain_commit(chains[0], 2, 10, hashes[0]));
  ck_assert(praef_comchain_is_dead(chains[0]));
}

deftest(can_create_commits_with_different_hashes) {
  unsigned char hash0[PRAEF_HASH_SIZE], hash1[PRAEF_HASH_SIZE];

  ck_assert(praef_comchain_reveal(chains[0], 0, hashes[0]));
  ck_assert(praef_comchain_create_commit(hash0, chains[0], 0, 1));
  ck_assert(praef_comchain_create_commit(hash1, chains[1], 0, 1));
  ck_assert_int_eq(1, praef_comchain_committed(chains[0]));
  ck_assert_int_eq(1, praef_comchain_committed(chains[1]));
  ck_assert_int_eq(1, praef_comchain_validated(chains[0]));
  ck_assert_int_eq(1, praef_comchain_validated(chains[1]));
  ck_assert(!praef_comchain_is_dead(chains[0]));
  ck_assert(!praef_comchain_is_dead(chains[1]));
  ck_assert_int_ne(0, memcmp(hash0, hash1, PRAEF_HASH_SIZE));
}

deftest(predicted_hashes_are_consistent_wort_ordering) {
  unsigned char hash0[PRAEF_HASH_SIZE], hash1[PRAEF_HASH_SIZE];

  ck_assert(praef_comchain_reveal(chains[0], 0, hashes[0]));
  ck_assert(praef_comchain_reveal(chains[0], 0, hashes[1]));
  ck_assert(praef_comchain_reveal(chains[1], 0, hashes[1]));
  ck_assert(praef_comchain_reveal(chains[1], 0, hashes[0]));
  ck_assert(praef_comchain_create_commit(hash0, chains[0], 0, 1));
  ck_assert(praef_comchain_create_commit(hash1, chains[1], 0, 1));
  ck_assert_int_eq(0, memcmp(hash0, hash1, PRAEF_HASH_SIZE));
}

deftest(correct_objects_collected_on_create_commit) {
  unsigned char hash0[PRAEF_HASH_SIZE], hash1[PRAEF_HASH_SIZE];

  ck_assert(praef_comchain_reveal(chains[0], 1, hashes[1]));
  ck_assert(praef_comchain_reveal(chains[1], 0, hashes[0]));
  ck_assert(praef_comchain_reveal(chains[1], 1, hashes[1]));
  ck_assert(praef_comchain_reveal(chains[1], 2, hashes[2]));
  ck_assert(praef_comchain_create_commit(hash0, chains[0], 1, 2));
  ck_assert(praef_comchain_create_commit(hash1, chains[1], 1, 2));
  ck_assert_int_eq(0, memcmp(hash0, hash1, PRAEF_HASH_SIZE));
}

deftest(create_commit_fails_and_invalidates_on_simultaneous_conflict) {
  unsigned char hash[PRAEF_HASH_SIZE];

  ck_assert(praef_comchain_create_commit(hash, chains[0], 0, 1));
  ck_assert(!praef_comchain_create_commit(hash, chains[0], 0, 2));
  ck_assert(praef_comchain_is_dead(chains[0]));
}

deftest(create_commit_fails_and_invalidates_on_past_overlapping_commit) {
  unsigned char hash[PRAEF_HASH_SIZE];

  ck_assert(praef_comchain_create_commit(hash, chains[0], 0, 10));
  ck_assert(!praef_comchain_create_commit(hash, chains[0], 5, 15));
  ck_assert(praef_comchain_is_dead(chains[0]));
}

deftest(create_commit_fails_and_invalidates_on_future_overlapping_commit) {
  unsigned char hash[PRAEF_HASH_SIZE];

  ck_assert(praef_comchain_create_commit(hash, chains[0], 5, 15));
  ck_assert(!praef_comchain_create_commit(hash, chains[0], 0, 10));
  ck_assert(praef_comchain_is_dead(chains[0]));
}

deftest(linear_validated_threshold_advances) {
  unsigned char hash[3][PRAEF_HASH_SIZE];

  praef_comchain_reveal(chains[0], 0, hashes[0]);
  praef_comchain_reveal(chains[0], 1, hashes[1]);
  praef_comchain_reveal(chains[0], 1, hashes[2]);
  praef_comchain_reveal(chains[0], 2, hashes[3]);
  praef_comchain_create_commit(hash[0], chains[0], 0, 1);
  praef_comchain_create_commit(hash[1], chains[0], 1, 2);
  praef_comchain_create_commit(hash[2], chains[0], 2, 3);
  ck_assert_int_eq(3, praef_comchain_committed(chains[0]));
  ck_assert_int_eq(3, praef_comchain_validated(chains[0]));

  praef_comchain_commit(chains[1], 0, 1, hash[0]);
  praef_comchain_commit(chains[1], 1, 2, hash[1]);
  praef_comchain_commit(chains[1], 2, 3, hash[2]);
  ck_assert_int_eq(3, praef_comchain_committed(chains[1]));
  ck_assert_int_eq(0, praef_comchain_validated(chains[1]));
  praef_comchain_reveal(chains[1], 0, hashes[0]);
  ck_assert_int_eq(1, praef_comchain_validated(chains[1]));
  praef_comchain_reveal(chains[1], 1, hashes[1]);
  ck_assert_int_eq(1, praef_comchain_validated(chains[1]));
  praef_comchain_reveal(chains[1], 1, hashes[2]);
  ck_assert_int_eq(2, praef_comchain_validated(chains[1]));
  praef_comchain_reveal(chains[1], 2, hashes[3]);
  ck_assert_int_eq(3, praef_comchain_validated(chains[1]));
}

deftest(nonlinear_validated_threshold_advances) {
  unsigned char hash[3][PRAEF_HASH_SIZE];

  praef_comchain_reveal(chains[0], 0, hashes[0]);
  praef_comchain_reveal(chains[0], 1, hashes[1]);
  praef_comchain_reveal(chains[0], 1, hashes[2]);
  praef_comchain_reveal(chains[0], 2, hashes[3]);
  praef_comchain_create_commit(hash[0], chains[0], 0, 1);
  praef_comchain_create_commit(hash[1], chains[0], 1, 2);
  praef_comchain_create_commit(hash[2], chains[0], 2, 3);
  ck_assert_int_eq(3, praef_comchain_committed(chains[0]));
  ck_assert_int_eq(3, praef_comchain_validated(chains[0]));

  praef_comchain_commit(chains[1], 0, 1, hash[0]);
  praef_comchain_commit(chains[1], 1, 2, hash[1]);
  praef_comchain_commit(chains[1], 2, 3, hash[2]);
  ck_assert_int_eq(3, praef_comchain_committed(chains[1]));
  ck_assert_int_eq(0, praef_comchain_validated(chains[1]));
  praef_comchain_reveal(chains[1], 1, hashes[2]);
  ck_assert_int_eq(0, praef_comchain_validated(chains[1]));
  praef_comchain_reveal(chains[1], 2, hashes[3]);
  ck_assert_int_eq(0, praef_comchain_validated(chains[1]));
  praef_comchain_reveal(chains[1], 0, hashes[0]);
  ck_assert_int_eq(1, praef_comchain_validated(chains[1]));
  praef_comchain_reveal(chains[1], 1, hashes[1]);
  ck_assert_int_eq(3, praef_comchain_validated(chains[1]));
}

deftest(empty_commit_becomes_valid_immediately) {
  unsigned char hash[PRAEF_HASH_SIZE];

  praef_comchain_create_commit(hash, chains[0], 0, 1);
  praef_comchain_commit(chains[1], 0, 1, hash);
  ck_assert_int_eq(1, praef_comchain_committed(chains[1]));
  ck_assert_int_eq(1, praef_comchain_validated(chains[1]));
}

deftest(commits_can_be_invalidated) {
  unsigned char hash[PRAEF_HASH_SIZE];

  praef_comchain_reveal(chains[0], 0, hashes[0]);
  praef_comchain_create_commit(hash, chains[0], 0, 1);
  praef_comchain_commit(chains[1], 0, 1, hash);
  praef_comchain_reveal(chains[1], 0, hashes[0]);
  ck_assert_int_eq(1, praef_comchain_committed(chains[1]));
  ck_assert_int_eq(1, praef_comchain_validated(chains[1]));
  ck_assert(!praef_comchain_is_dead(chains[1]));
  praef_comchain_reveal(chains[1], 0, hashes[1]);
  ck_assert_int_eq(0, praef_comchain_validated(chains[1]));
  ck_assert(praef_comchain_is_dead(chains[1]));
}

deftest(duplicate_objects_in_commit_invalidate) {
  praef_comchain_commit(chains[0], 0, 2, hashes[0]);
  praef_comchain_reveal(chains[0], 0, hashes[1]);
  praef_comchain_reveal(chains[0], 1, hashes[1]);
  ck_assert(praef_comchain_is_dead(chains[0]));
}

deftest(duplicate_unassoc_objects_invalidate) {
  praef_comchain_reveal(chains[0], 0, hashes[0]);
  praef_comchain_reveal(chains[0], 0, hashes[0]);
  ck_assert(praef_comchain_is_dead(chains[0]));
}

deftest(duplicate_unassoc_objects_invalidate_when_added_to_commit) {
  praef_comchain_reveal(chains[0], 0, hashes[0]);
  praef_comchain_reveal(chains[0], 1, hashes[0]);
  ck_assert(!praef_comchain_is_dead(chains[0]));
  praef_comchain_commit(chains[0], 0, 2, hashes[1]);
  ck_assert(praef_comchain_is_dead(chains[0]));
}

deftest(unassoc_objects_are_freed) {
  /* This test doesn't assert anything; it is here so that we can easily detect
   * whether unassociated objects lingering until the deletion of the comchain
   * are actually freed by running under valgrind.
   */
  praef_comchain_reveal(chains[0], 0, hashes[0]);
}

deftest(valid_invalid_commits_coalesce_to_invalid) {
  unsigned char hash[PRAEF_HASH_SIZE];

  praef_comchain_reveal(chains[0], 0, hashes[0]);
  praef_comchain_create_commit(hash, chains[0], 0, 2);
  praef_comchain_commit(chains[1], 0, 2, hash);
  praef_comchain_commit(chains[1], 2, 3, hash);
  praef_comchain_reveal(chains[1], 2, hashes[0]);
  praef_comchain_reveal(chains[1], 2, hashes[1]);
  praef_comchain_reveal(chains[1], 1, hashes[0]);
  ck_assert_int_eq(0, praef_comchain_validated(chains[1]));
  ck_assert(praef_comchain_is_dead(chains[1]));
}

deftest(invalid_valid_commits_coalesce_to_invalid) {
  unsigned char hash[PRAEF_HASH_SIZE];

  praef_comchain_reveal(chains[0], 0, hashes[0]);
  praef_comchain_create_commit(hash, chains[0], 0, 2);
  praef_comchain_commit(chains[1], 0, 2, hash);
  praef_comchain_commit(chains[1], 2, 3, hash);
  praef_comchain_reveal(chains[1], 0, hashes[0]);
  ck_assert_int_eq(2, praef_comchain_validated(chains[1]));
  praef_comchain_reveal(chains[1], 0, hashes[1]);
  praef_comchain_reveal(chains[1], 2, hashes[0]);
  ck_assert_int_eq(0, praef_comchain_validated(chains[1]));
  ck_assert(praef_comchain_is_dead(chains[1]));
}

deftest(can_add_object_to_final_commit_beyond_start) {
  unsigned char hash[PRAEF_HASH_SIZE];

  praef_comchain_reveal(chains[0], 2, hashes[0]);
  praef_comchain_create_commit(hash, chains[0], 0, 5);

  praef_comchain_commit(chains[1], 0, 5, hash);
  praef_comchain_reveal(chains[1], 2, hashes[0]);
  ck_assert_int_eq(5, praef_comchain_validated(chains[1]));
}
