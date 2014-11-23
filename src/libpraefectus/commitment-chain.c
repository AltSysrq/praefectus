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

#include <string.h>

#include "keccak.h"
#include "common.h"
#include "commitment-chain.h"

/**
 * An object that is either not yet part of a commit, or is part of a pending
 * commit.
 */
typedef struct praef_comchain_object_s {
  /**
   * The hash of this object.
   */
  unsigned char hash[PRAEF_HASH_SIZE];
  /**
   * The instant of this object, used to identify which commit owns it in the
   * case where the commit comes into existence after the object.
   */
  praef_instant instant;

  /**
   * Entry for a RB-tree. Depending on the tree in which the object lives, this
   * may be keyed on the hash or the instant.
   */
  RB_ENTRY(praef_comchain_object_s) tree;
} praef_comchain_object;

static int praef_compare_comchain_object_by_hash(
  const praef_comchain_object* a,
  const praef_comchain_object* b
) {
  return memcmp(a->hash, b->hash, PRAEF_HASH_SIZE);
}

static int praef_compare_comchain_object_by_instant(
  const praef_comchain_object* a,
  const praef_comchain_object* b
) {
  if (a->instant < b->instant) return -1;
  if (a->instant > b->instant) return +1;
  return praef_compare_comchain_object_by_hash(a, b);
}

RB_HEAD(praef_comchain_object_hash_tree, praef_comchain_object_s);
RB_PROTOTYPE_STATIC(praef_comchain_object_hash_tree,
                    praef_comchain_object_s, tree,
                    praef_compare_comchain_object_by_hash);
RB_GENERATE_STATIC(praef_comchain_object_hash_tree,
                   praef_comchain_object_s, tree,
                   praef_compare_comchain_object_by_hash);
RB_HEAD(praef_comchain_object_instant_tree, praef_comchain_object_s);
RB_PROTOTYPE_STATIC(praef_comchain_object_instant_tree,
                    praef_comchain_object_s, tree,
                    praef_compare_comchain_object_by_instant);
RB_GENERATE_STATIC(praef_comchain_object_instant_tree,
                   praef_comchain_object_s, tree,
                   praef_compare_comchain_object_by_instant);

/**
 * Possible states for a commit.
 */
typedef enum {
  /**
   * Indicates that the commit has not yet entered a valid state. The commit's
   * object tree is meaningful.
   */
  praef_ccs_pending,
  /**
   * Indicates that the commit has entered the valid state. The commit's object
   * tree is empty, since it is no longer necessary to track this information.
   */
  praef_ccs_validated,
  /**
   * Indicates that the commit has entered and left the valid state,
   * permantently. The object tree is empty and meaningless since the validated
   * state discards it (and keeping it would be useless anyway, since there
   * will never be a return to the valid state).
   */
  praef_ccs_invalidated
} praef_comchain_commitment_status;

/**
 * Tracks the state for a single commit. Note that validated contiguous commits
 * are coalesced to keep the tree small.
 */
typedef struct praef_comchain_commitment_s {
  /**
   * The current status of this commit.
   */
  praef_comchain_commitment_status status;
  /**
   * The inclusive start and exclusive end times of this commit, respectively.
   */
  praef_instant start, end;
  /**
   * The expected valid second-order hash of this commit. The hash is computed
   * by inputting the hashes of all component objects, sorted ascending by hash
   * value, through the Keccak function. This field is not meaningful outside
   * of the pending status.
   */
  unsigned char hash[PRAEF_HASH_SIZE];

  /**
   * A tree of objects, by hash, which are part of this commit. This field is
   * only meaningful in the pending status.
   */
  struct praef_comchain_object_hash_tree pending_objects;

  /**
   * Tree entry, used to sort commits by starting time.
   */
  RB_ENTRY(praef_comchain_commitment_s) tree;
} praef_comchain_commitment;

static int praef_compare_comchain_commitment(
  const praef_comchain_commitment* a,
  const praef_comchain_commitment* b
) {
  if (a->start < b->start) return -1;
  if (a->start > b->start) return +1;
  return 0;
}

RB_HEAD(praef_comchain_commitment_tree, praef_comchain_commitment_s);
RB_PROTOTYPE_STATIC(praef_comchain_commitment_tree,
                    praef_comchain_commitment_s, tree,
                    praef_compare_comchain_commitment);
RB_GENERATE_STATIC(praef_comchain_commitment_tree,
                   praef_comchain_commitment_s, tree,
                   praef_compare_comchain_commitment);

struct praef_comchain_s {
  /**
   * The commits in this comchain, sorted ascending by start instant.
   */
  struct praef_comchain_commitment_tree commits;
  /**
   * Objects which have been revealed, but for which there is currently no
   * commit. Sorted ascending by instant, then by hash.
   */
  struct praef_comchain_object_instant_tree unassociated_objects;
  /**
   * If set, the comchain has entered a state of permanent invalidity. This
   * renders every operation against the comchain a no-op.
   */
  int invalid;
};

praef_comchain* praef_comchain_new(void) {
  praef_comchain* this = malloc(sizeof(praef_comchain));
  if (!this) return NULL;

  RB_INIT(&this->commits);
  RB_INIT(&this->unassociated_objects);
  this->invalid = 0;
  return this;
}

static praef_comchain_commitment*
praef_comchain_commitment_new(praef_instant start,
                              praef_instant end,
                              const unsigned char hash[PRAEF_HASH_SIZE]) {
  praef_comchain_commitment* commit = malloc(sizeof(praef_comchain_commitment));
  if (!commit) return NULL;

  commit->start = start;
  commit->end = end;
  memcpy(commit->hash, hash, PRAEF_HASH_SIZE);
  RB_INIT(&commit->pending_objects);
  commit->status = praef_ccs_pending;

  return commit;
}

static void praef_comchain_commitment_clear(praef_comchain_commitment* this) {
  praef_comchain_object* obj, * tmp;

  for (obj = RB_MIN(praef_comchain_object_hash_tree, &this->pending_objects);
       obj; obj = tmp) {
    tmp = RB_NEXT(praef_comchain_object_hash_tree, &this->pending_objects, obj);
    RB_REMOVE(praef_comchain_object_hash_tree, &this->pending_objects, obj);
    free(obj);
  }
}

void praef_comchain_delete(praef_comchain* this) {
  praef_comchain_object* obj, * objtmp;
  praef_comchain_commitment* com, * comtmp;

  for (obj = RB_MIN(praef_comchain_object_instant_tree,
                    &this->unassociated_objects);
       obj; obj = objtmp) {
    objtmp = RB_NEXT(praef_comchain_object_instant_tree,
                     &this->unassociated_objects, obj);
    RB_REMOVE(praef_comchain_object_instant_tree,
              &this->unassociated_objects, obj);
    free(obj);
  }

  for (com = RB_MIN(praef_comchain_commitment_tree, &this->commits);
       com; com = comtmp) {
    comtmp = RB_NEXT(praef_comchain_commitment_tree,
                     &this->commits, com);
    RB_REMOVE(praef_comchain_commitment_tree, &this->commits, com);
    praef_comchain_commitment_clear(com);
    free(com);
  }

  free(this);
}

/**
 * Adds an object to the given commit without recomputing the hash. The object
 * becomes owned by this call. Returns whether the commit needs to be rehashed.
 */
static int praef_comchain_commitment_add_object_without_rehash(
  praef_comchain_commitment* this,
  praef_comchain_object* obj
) {
  if (praef_ccs_pending != this->status) {
    /* Nothing to do, other than to ensure in invalidated state */
    this->status = praef_ccs_invalidated;
    free(obj);
    return 0;
  }

  if (RB_INSERT(praef_comchain_object_hash_tree, &this->pending_objects, obj)) {
    /* Such an object is already in the tree. Free the memory and invalidate
     * the commit.
     */
    free(obj);
    this->status = praef_ccs_invalidated;
    praef_comchain_commitment_clear(this);
    return 0;
  }

  return 1;
}

static void praef_comchain_commitment_calc_hash(
  unsigned char hash[PRAEF_HASH_SIZE],
  praef_comchain_commitment* this
) {
  praef_keccak_sponge sponge;
  praef_comchain_object* object;

  praef_keccak_sponge_init(&sponge, PRAEF_KECCAK_RATE, PRAEF_KECCAK_CAP);
  RB_FOREACH(object, praef_comchain_object_hash_tree, &this->pending_objects)
    praef_keccak_sponge_absorb(&sponge, object->hash, PRAEF_HASH_SIZE);

  praef_keccak_sponge_squeeze(&sponge, hash, PRAEF_HASH_SIZE);
}

/**
 * If in the pending state, rehashes the contents of the given commit. If this
 * matches the expected hash, moves the commit into the validated state.
 */
static void praef_comchain_commitment_rehash(praef_comchain_commitment* this) {
  unsigned char hash[PRAEF_HASH_SIZE];

  if (praef_ccs_pending != this->status) return;

  praef_comchain_commitment_calc_hash(hash, this);
  if (!memcmp(hash, this->hash, PRAEF_HASH_SIZE)) {
    praef_comchain_commitment_clear(this);
    this->status = praef_ccs_validated;
  }
}

/**
 * Adds (without rehashing) to the given commit all unassociated objects in the
 * given comchain which should belong to the commit, removing them from the
 * comchain itself.
 */
static void praef_comchain_commitment_backfill(
  praef_comchain* this,
  praef_comchain_commitment* commit
) {
  praef_comchain_object* obj, * objtmp, exobj;

  exobj.instant = commit->start;
  memset(exobj.hash, 0, PRAEF_HASH_SIZE);
  for (obj = RB_NFIND_CMP(praef_comchain_object_instant_tree,
                          &this->unassociated_objects, &exobj,
                          tree, praef_compare_comchain_object_by_instant);
       obj && obj->instant < commit->end; obj = objtmp) {
    objtmp = RB_NEXT(praef_comchain_object_instant_tree,
                     &this->unassociated_objects, obj);
    RB_REMOVE(praef_comchain_object_instant_tree,
              &this->unassociated_objects, obj);
    praef_comchain_commitment_add_object_without_rehash(commit, obj);
  }
}

static int praef_comchain_insert_commit(praef_comchain* this,
                                        praef_comchain_commitment* commit) {
  praef_comchain_commitment* other;
  int has_been_inserted = 0;

  if (RB_INSERT(praef_comchain_commitment_tree, &this->commits, commit))
    /* Another commit already has this start point, so overlap is
     * guaranteed. Invalidate the comchain and pretend we were successful.
     */
    goto invalidate;

  has_been_inserted = 1;

  /* Check the immediately preceding and proceeding commits for overlap. This
   * check breaks if there already is overlap, but in such a case the comchain
   * is already considered dead, so this point is never reached.
   */
  other = RB_PREV(praef_comchain_commitment_tree, &this->commits, commit);
  if (other && other->end > commit->start) goto invalidate;
  other = RB_NEXT(praef_comchain_commitment_tree, &this->commits, commit);
  if (other && commit->end > other->start) goto invalidate;

  return 1;

  invalidate:
  /* Only free the commit if we haven't already added it to the tree. If it is
   * in the tree already, leave it there and clean it up when the comchain is
   * destroyed.
   */
  if (!has_been_inserted) free(commit);
  this->invalid = 1;
  return 0;
}

static praef_comchain_commitment*
praef_comchain_commitment_coalesce_pair(
  praef_comchain* this,
  praef_comchain_commitment* left,
  praef_comchain_commitment* right);
/**
 * For each immediate neighbour of the given centre commit, if that neighbour
 * is contiguous with this commit, and neither is in the pending state,
 * collapse them into one commit. The resulting status is validated if both
 * originally had the validated status, and is invalidated if either was in the
 * invalidated state.
 */
static praef_comchain_commitment* praef_comchain_commitment_coalesce(
  praef_comchain* this, praef_comchain_commitment* centre
) {
  praef_comchain_commitment_coalesce_pair(
    this, centre,
    RB_NEXT(praef_comchain_commitment_tree, &this->commits, centre));
  return
    praef_comchain_commitment_coalesce_pair(
      this, RB_PREV(praef_comchain_commitment_tree, &this->commits, centre),
      centre);
}

static praef_comchain_commitment*
praef_comchain_commitment_coalesce_pair(
  praef_comchain* this,
  praef_comchain_commitment* left,
  praef_comchain_commitment* right
) {
  if (left && right &&
      left->end == right->start &&
      praef_ccs_pending != left->status &&
      praef_ccs_pending != right->status) {
    left->end = right->end;
    if (praef_ccs_invalidated == right->status)
      left->status = praef_ccs_invalidated;

    RB_REMOVE(praef_comchain_commitment_tree, &this->commits, right);
    free(right);
    return left;
  }

  return right;
}

int praef_comchain_commit(praef_comchain* this,
                          praef_instant start,
                          praef_instant end,
                          const unsigned char hash[PRAEF_HASH_SIZE]) {
  praef_comchain_commitment* commit;

  /* If in permanent failure, just pretend to succeed, since anything goes at
   * this point, and failure normally means out of memory.
   */
  if (this->invalid) return 1;

  commit = praef_comchain_commitment_new(start, end, hash);
  if (!commit) return 0;

  if (!praef_comchain_insert_commit(this, commit))
    /* Invalidated. Pretend successful. */
    return 1;

  /* The commit is acceptable as far as this function is concerned. Find
   * unassociated objects that belong to it and add them.
   */
  praef_comchain_commitment_backfill(this, commit);
  praef_comchain_commitment_rehash(commit);
  commit = praef_comchain_commitment_coalesce(this, commit);
  this->invalid |= (praef_ccs_invalidated == commit->status);

  return 1;
}

int praef_comchain_reveal(praef_comchain* this,
                          praef_instant instant,
                          const unsigned char hash[PRAEF_HASH_SIZE]) {
  praef_comchain_commitment example, * commit;
  praef_comchain_object* object;

  object = malloc(sizeof(praef_comchain_object));
  if (!object) return 0;

  object->instant = instant;
  memcpy(object->hash, hash, PRAEF_HASH_SIZE);

  /* Find the commit that owns this commit. NFIND will give us the first commit
   * *past* the commit's time unless the start happens to be the same as this
   * instant, so use the one before that if needed. If NFIND returns NULL, use
   * the final commit in the tree.
   *
   * Either case still requires an explicit check afterwards to see whether the
   * object really falls within the commit's range.
   */
  example.start = instant;
  commit = RB_NFIND_CMP(praef_comchain_commitment_tree,
                        &this->commits, &example,
                        tree, praef_compare_comchain_commitment);
  if (commit && commit->start > instant)
    commit = RB_PREV(praef_comchain_commitment_tree, &this->commits, commit);
  else if (!commit)
    commit = RB_MAX(praef_comchain_commitment_tree, &this->commits);

  if (commit && instant >= commit->start && instant < commit->end) {
    if (praef_comchain_commitment_add_object_without_rehash(commit, object)) {
      praef_comchain_commitment_rehash(commit);
      commit = praef_comchain_commitment_coalesce(this, commit);
    }
    this->invalid |= (praef_ccs_invalidated == commit->status);
  } else {
    /* No applicable commit yet; add to unassociated tree. */
    if (RB_INSERT(praef_comchain_object_instant_tree,
                  &this->unassociated_objects, object)) {
      /* Duplicate */
      free(object);
      this->invalid = 1;
    }
  }

  return 1;
}

int praef_comchain_is_dead(const praef_comchain* this) {
  return this->invalid;
}

praef_instant praef_comchain_committed(const praef_comchain* cthis) {
  /* The RB prototypes aren't const-correct */
  praef_comchain* this = (praef_comchain*)cthis;
  praef_comchain_commitment* commit, * next;

  commit = RB_MIN(praef_comchain_commitment_tree, &this->commits);
  if (!commit || 0 != commit->start)
    /* No commit at time zero */
    return 0;

  /* Walk forward across contiguous commits until the end of the commits list
   * is found, or a commit discontiguous from the previous is encountered.
   */
  for (next = RB_NEXT(praef_comchain_commitment_tree, &this->commits, commit);
       next && commit->end == next->start;
       next = RB_NEXT(praef_comchain_commitment_tree, &this->commits, commit))
    commit = next;

  return commit->end;
}

praef_instant praef_comchain_validated(const praef_comchain* cthis) {
  /* The RB prototypes aren't const-correct */
  praef_comchain* this = (praef_comchain*)cthis;
  const praef_comchain_commitment* commit;

  commit = RB_MIN(praef_comchain_commitment_tree, &this->commits);
  /* Since we always coalesce commits that leave the pending state, we only
   * need to look at the earliest event, as the coalescence guarantees that any
   * event that follows it is still pending.
   */
  if (commit && 0 == commit->start && praef_ccs_validated == commit->status)
    return commit->end;
  else
    return 0;
}

int praef_comchain_create_commit(unsigned char hash[PRAEF_HASH_SIZE],
                                 praef_comchain* this,
                                 praef_instant start,
                                 praef_instant end) {
  praef_comchain_commitment* commit;

  commit = praef_comchain_commitment_new(start, end,
                                         /* hash is currently garbage, but
                                          * we're about to fill it in.
                                          */
                                         hash);
  if (!commit) return 0;

  if (!praef_comchain_insert_commit(this, commit))
    return 0;

  praef_comchain_commitment_backfill(this, commit);
  praef_comchain_commitment_calc_hash(hash, commit);
  memcpy(commit->hash, hash, PRAEF_HASH_SIZE);
  praef_comchain_commitment_rehash(commit);
  praef_comchain_commitment_coalesce(this, commit);
  return 1;
}
