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
#include <stddef.h>
#include <assert.h>

#include "keccak.h"
#include "hash-tree.h"
#include "defs.h"

typedef struct praef_hash_tree_fulldir_s praef_hash_tree_fulldir;
struct praef_hash_tree_fulldir_s {
  praef_hash_tree_directory directory;
  praef_hash_tree_fulldir* subdirectories[PRAEF_HTDIR_SIZE];
  unsigned refcount;
};

static praef_hash_tree_fulldir* praef_hash_tree_fulldir_new(void);
static void praef_hash_tree_fulldir_decref(praef_hash_tree_fulldir*);
static int praef_hash_tree_fulldir_fork(praef_hash_tree_fulldir**);

typedef struct {
  unsigned char hash[PRAEF_HASH_SIZE];
  unsigned size;
  praef_instant instant;
  unsigned char data[FLEXIBLE_ARRAY_MEMBER];
} praef_hash_tree_object;

static praef_hash_tree_object* praef_hash_tree_object_new(
  const praef_hash_tree_objref*, const unsigned char hash[PRAEF_HASH_SIZE]);

typedef struct {
  praef_hash_tree_object** objects;
  unsigned capacity;
  unsigned refcount;
} praef_hash_tree_objtab;

static praef_hash_tree_objtab* praef_hash_tree_objtab_new(void);
static void praef_hash_tree_objtab_decref(praef_hash_tree_objtab*);
static int praef_hash_tree_objtab_push_back(
  unsigned*, praef_hash_tree_objtab*, praef_hash_tree_object*);

struct praef_hash_tree_s {
  praef_hash_tree_fulldir* root;
  praef_hash_tree_objtab* object_table;
  unsigned next_object_id;
};

praef_hash_tree* praef_hash_tree_new(void) {
  praef_hash_tree* this = calloc(1, sizeof(praef_hash_tree));
  if (!this) return NULL;

  this->root = praef_hash_tree_fulldir_new();
  this->object_table = praef_hash_tree_objtab_new();
  if (!this->root || !this->object_table) goto fail;

  return this;

  fail:
  if (this->root) praef_hash_tree_fulldir_decref(this->root);
  if (this->object_table) praef_hash_tree_objtab_decref(this->object_table);
  free(this);
  return NULL;
}

const praef_hash_tree* praef_hash_tree_fork(const praef_hash_tree* orig) {
  praef_hash_tree* this = malloc(sizeof(praef_hash_tree));
  if (!this) return NULL;

  memcpy(this, orig, sizeof(praef_hash_tree));
  ++this->root->refcount;
  ++this->object_table->refcount;
  return this;
}

void praef_hash_tree_delete(const praef_hash_tree* this) {
  praef_hash_tree_fulldir_decref(this->root);
  praef_hash_tree_objtab_decref(this->object_table);
  free((praef_hash_tree*)this);
}

static praef_hash_tree_fulldir* praef_hash_tree_fulldir_new(void) {
  praef_hash_tree_fulldir* this = calloc(1, sizeof(praef_hash_tree_fulldir));
  if (!this) return NULL;

  this->refcount = 1;
  return this;
}

static int praef_hash_tree_fulldir_fork(praef_hash_tree_fulldir** thisp) {
  praef_hash_tree_fulldir* shared, * unique;
  unsigned i;

  if (1 == (*thisp)->refcount)
    /* Unshared, so no action is necessary */
    return 1;

  /* Shared. Need to create a fresh copy that can be mutated. */
  shared = *thisp;
  unique = malloc(sizeof(praef_hash_tree_fulldir));
  if (!unique) return 0;

  memcpy(unique, shared, sizeof(praef_hash_tree_fulldir));
  unique->refcount = 1;
  --shared->refcount;

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i)
    if (unique->subdirectories[i])
      ++unique->subdirectories[i]->refcount;

  *thisp = unique;
  return 1;
}

static void praef_hash_tree_fulldir_decref(praef_hash_tree_fulldir* this) {
  unsigned i;

  if (--this->refcount) return;

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i)
    if (this->subdirectories[i])
      praef_hash_tree_fulldir_decref(this->subdirectories[i]);

  free(this);
}

static praef_hash_tree_object* praef_hash_tree_object_new(
  const praef_hash_tree_objref* ref,
  const unsigned char hash[PRAEF_HASH_SIZE]
) {
  praef_hash_tree_object* this = malloc(offsetof(praef_hash_tree_object, data) +
                                        ref->size + 1);
  if (!this) return NULL;

  memcpy(this->hash, hash, PRAEF_HASH_SIZE);
  this->size = ref->size;
  this->instant = ref->instant;
  memcpy(this->data, ref->data, ref->size);
  this->data[this->size] = 0;
  return this;
}

static praef_hash_tree_objtab* praef_hash_tree_objtab_new(void) {
  praef_hash_tree_objtab* this = calloc(1, sizeof(praef_hash_tree_objtab));
  if (!this) return NULL;

  this->capacity = 16;
  this->refcount = 1;
  this->objects = calloc(this->capacity, sizeof(praef_hash_tree_object*));
  if (!this->objects) {
    free(this);
    return NULL;
  }

  return this;
}

static void praef_hash_tree_objtab_decref(praef_hash_tree_objtab* this) {
  unsigned i;

  if (--this->refcount) return;

  for (i = 0; i < this->capacity; ++i)
    if (this->objects[i])
      free(this->objects[i]);

  free(this->objects);
  free(this);
}

static int praef_hash_tree_objtab_push_back(
  unsigned* id, praef_hash_tree_objtab* this,
  praef_hash_tree_object* object
) {
  praef_hash_tree_object** new_objects;

  assert(*id <= this->capacity);

  if (*id == this->capacity) {
    new_objects = realloc(
      this->objects, this->capacity * 2 * sizeof(praef_hash_tree_object*));
    if (!new_objects) return 0;

    this->objects = new_objects;

    memset(new_objects + this->capacity, 0,
           this->capacity * sizeof(praef_hash_tree_object*));
    this->capacity *= 2;
  }

  this->objects[(*id)++] = object;
  return 1;
}

static inline unsigned char nybble(const unsigned char* in,
                                   unsigned ix) {
  return (in[ix/2] >> (((ix&1)^1)*4)) & 0x0F;
}

static praef_hash_tree_add_result
praef_hash_tree_add_to(praef_hash_tree_fulldir**,
                       praef_hash_tree_objref*,
                       const unsigned char hash[PRAEF_HASH_SIZE],
                       unsigned, praef_hash_tree*);

praef_hash_tree_add_result
praef_hash_tree_add(praef_hash_tree* this, praef_hash_tree_objref* ref) {
  unsigned char hash[PRAEF_HASH_SIZE];
  praef_keccak_sponge sponge;

  praef_sha3_init(&sponge);
  praef_keccak_sponge_absorb(&sponge, ref->data, ref->size);
  praef_keccak_sponge_squeeze(&sponge, hash, PRAEF_HASH_SIZE);

  return praef_hash_tree_add_to(&this->root, ref, hash, 0, this);
}

static void praef_hash_tree_rehash(praef_hash_tree_fulldir*, unsigned,
                                   const praef_hash_tree*);

static praef_hash_tree_add_result
praef_hash_tree_add_to(praef_hash_tree_fulldir** thisp,
                       praef_hash_tree_objref* ref,
                       const unsigned char hash[PRAEF_HASH_SIZE],
                       unsigned offset, praef_hash_tree* tree) {
  unsigned ix = nybble(hash, offset);
  unsigned id = tree->next_object_id;
  praef_hash_tree_object* object;
  praef_hash_tree_fulldir* subdir;
  praef_hash_tree_add_result result;

  switch ((*thisp)->directory.types[ix]) {
  case praef_htet_none:
    /* Simple insertion */
    object = praef_hash_tree_object_new(ref, hash);
    if (!object) return praef_htar_failed;

    if (!praef_hash_tree_fulldir_fork(thisp)) {
      free(object);
      return praef_htar_failed;
    }

    if (!praef_hash_tree_objtab_push_back(
          &tree->next_object_id, tree->object_table, object)) {
      free(object);
      return praef_htar_failed;
    }

    (*thisp)->directory.types[ix] = praef_htet_object;
    (*thisp)->directory.sids[ix] = id;
    ref->data = object->data;
    return praef_htar_added;

  case praef_htet_object:
    if (0 == memcmp(hash, tree->object_table->objects[
                      (*thisp)->directory.sids[ix]]->hash,
                    PRAEF_HASH_SIZE)) {
      /* Identical object, nothing to do */
      ref->data = tree->object_table->objects[
        (*thisp)->directory.sids[ix]]->data;
      return praef_htar_already_present;
    }

    /* Convert inline object to singleton directory */
    if (!praef_hash_tree_fulldir_fork(thisp)) return praef_htar_failed;
    subdir = praef_hash_tree_fulldir_new();
    if (!subdir) return praef_htar_failed;
    object = tree->object_table->objects[(*thisp)->directory.sids[ix]];
    subdir->directory.types[nybble(object->hash, offset+1)] =
      praef_htet_object;
    subdir->directory.sids [nybble(object->hash, offset+1)] =
      (*thisp)->directory.sids[ix];
    (*thisp)->directory.types[ix] = praef_htet_directory;
    (*thisp)->subdirectories[ix] = subdir;

    /* fall through */

  case praef_htet_directory:
    if (!praef_hash_tree_fulldir_fork(thisp)) return 0;

    result = praef_hash_tree_add_to(&(*thisp)->subdirectories[ix],
                                    ref, hash, offset+1, tree);
    praef_hash_tree_rehash(*thisp, ix, tree);
    return result;
  }

  /* Unreachable */
  assert(0);
  return 0;
}

static void praef_hash_tree_rehash(praef_hash_tree_fulldir* dir,
                                   unsigned subdir_ix,
                                   const praef_hash_tree* tree) {
  praef_hash_tree_fulldir* subdir = dir->subdirectories[subdir_ix];
  praef_keccak_sponge sponge;
  unsigned i;

  praef_sha3_init(&sponge);
  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    switch (subdir->directory.types[i]) {
    case praef_htet_none:
      /* Nothing to do */
      break;

    case praef_htet_object:
      praef_keccak_sponge_absorb(
        &sponge,
        tree->object_table->objects[subdir->directory.sids[i]]->hash,
        PRAEF_HASH_SIZE);
      break;

    case praef_htet_directory:
      praef_keccak_sponge_absorb_integer(
        &sponge, subdir->directory.sids[i], sizeof(praef_hash_tree_sid));
      break;
    }
  }

  dir->directory.sids[subdir_ix] = praef_keccak_sponge_squeeze_integer(
    &sponge, sizeof(praef_hash_tree_sid));
}

int praef_hash_tree_get_hash(praef_hash_tree_objref* dst,
                             const praef_hash_tree* this,
                             const unsigned char hash[PRAEF_HASH_SIZE]) {
  unsigned n, ix;
  const praef_hash_tree_fulldir* dir;

  for (n = 0, dir = this->root;; ++n) {
    ix = nybble(hash, n);
    switch (dir->directory.types[ix]) {
    case praef_htet_none:
      return 0;

    case praef_htet_object:
      return praef_hash_tree_get_id(dst, this, dir->directory.sids[ix]);

    case praef_htet_directory:
      dir = dir->subdirectories[ix];
      break;
    }
  }
}

int praef_hash_tree_get_id(praef_hash_tree_objref* dst,
                           const praef_hash_tree* this,
                           praef_hash_tree_sid id) {
  praef_hash_tree_object* object;

  if (id >= this->next_object_id) return 0;

  object = this->object_table->objects[id];
  dst->size = object->size;
  dst->instant = object->instant;
  dst->data = object->data;
  return 1;
}

/**
 * Recursively implements praef_hash_tree_get_range().
 *
 * @param dst The first destination that has not yet been written.
 * @param count The remaining entries in dst.
 * @param dir The directory to walk.
 * @param tree The tree being examined.
 * @param hash The input minimum hash.
 * @param hash_nybble The offset of this directory's hash nybble.
 * @param restrict_hash If true, only entries beginning at the index derived
 * from the hash and the hash offset are examined, and a subdirectory occurring
 * at this offset is also given this flag. Otherwise, all entries are
 * examined.
 * @param offset As per praef_hash_tree_get_range().
 * @param mask As per praef_hash_tree_get_range().
 * @return The number of entries that were read.
 */
static unsigned praef_hash_tree_get_range_from_dir(
  praef_hash_tree_objref* dst, unsigned count,
  const praef_hash_tree_fulldir* dir,
  const praef_hash_tree* tree,
  const unsigned char hash[PRAEF_HASH_SIZE],
  unsigned hash_nybble, int restrict_hash,
  unsigned char offset, unsigned char mask
) {
  unsigned i, init, n = 0, subn;
  const praef_hash_tree_object* object;

  if (restrict_hash)
    init = nybble(hash, hash_nybble);
  else
    init = 0;

  for (i = init; i < PRAEF_HTDIR_SIZE && count > 0; ++i) {
    switch (dir->directory.types[i]) {
    case praef_htet_none: break;
    case praef_htet_object:
      object = tree->object_table->objects[dir->directory.sids[i]];
      if (offset == (object->hash[PRAEF_HASH_SIZE-1] & mask)) {
        praef_hash_tree_get_id(dst, tree, dir->directory.sids[i]);
        ++dst;
        --count;
        ++n;
      }
      break;

    case praef_htet_directory:
      subn = praef_hash_tree_get_range_from_dir(
        dst, count, dir->subdirectories[i], tree, hash,
        hash_nybble+1, (restrict_hash && init == i), offset, mask);
      dst += subn;
      count -= subn;
      n += subn;
      break;
    }
  }

  return n;
}

unsigned praef_hash_tree_get_range(praef_hash_tree_objref* dst, unsigned count,
                                   const praef_hash_tree* tree,
                                   const unsigned char hash[PRAEF_HASH_SIZE],
                                   unsigned char offset, unsigned char mask) {
  return praef_hash_tree_get_range_from_dir(
    dst, count, tree->root, tree, hash, 0, 1, offset, mask);
}

const praef_hash_tree_directory* praef_hash_tree_readdir(
  const praef_hash_tree* tree, const praef_hash_tree_cursor* cursor
) {
  praef_hash_tree_fulldir* dir;
  unsigned n, ix;

  for (n = 0, dir = tree->root; n < cursor->offset; ++n) {
    ix = nybble(cursor->hash, n);
    if (praef_htet_directory != dir->directory.types[ix])
      /* No such directory */
      return 0;

    dir = dir->subdirectories[ix];
  }

  return &dir->directory;
}

unsigned praef_hash_tree_minimum_hash_length(
  const praef_hash_tree* tree, const unsigned char hash[PRAEF_HASH_SIZE]
) {
  praef_hash_tree_fulldir* dir;
  unsigned n, ix;

  for (n = 0, dir = tree->root; n < PRAEF_HASH_SIZE*2; ++n) {
    ix = nybble(hash, n);

    if (praef_htet_directory != dir->directory.types[ix])
      return n+1;
    else
      dir = dir->subdirectories[ix];
  }

  return n;
}

void praef_hash_tree_hash_of(unsigned char dst[PRAEF_HASH_SIZE],
                             const praef_hash_tree_objref* ref) {
  const praef_hash_tree_object* object =
    UNDOT(praef_hash_tree_object, data, ref->data);
  memcpy(dst, object->hash, PRAEF_HASH_SIZE);
}

const unsigned char* praef_hash_tree_get_hash_of(
  const praef_hash_tree_objref* ref
) {
  return UNDOT(praef_hash_tree_object, data, ref->data)->hash;
}
