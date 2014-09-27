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

#include <string.h>

#include <libpraefectus/hash-tree.h>

defsuite(libpraefectus_hash_tree);

static praef_hash_tree* tree;
defsetup {
  tree = praef_hash_tree_new();
}

defteardown {
  praef_hash_tree_delete(tree);
}

deftest(can_add_and_fetch_objects_by_id) {
  praef_hash_tree_objref object;
  unsigned i;

  for (i = 0; i < 65536; ++i) {
    object.size = sizeof(i);
    object.instant = i;
    object.data = &i;
    ck_assert_int_eq(praef_htar_added,
                     praef_hash_tree_add(tree, &object));
    ck_assert_ptr_ne(&i, object.data);
  }

  for (i = 0; i < 65536; ++i) {
    ck_assert(praef_hash_tree_get_id(&object, tree, i));
    ck_assert_int_eq(i, object.instant);
    ck_assert_int_eq(sizeof(i), object.size);
    ck_assert_int_eq(0, memcmp(&i, object.data, sizeof(i)));
  }
}

deftest(fetching_nonexistent_object_returns_zero) {
  praef_hash_tree_objref object;

  object.instant = 0;
  object.size = 0;
  object.data = NULL;
  ck_assert(praef_hash_tree_add(tree, &object));
  ck_assert_ptr_ne(NULL, object.data);
  ck_assert(praef_hash_tree_get_id(&object, tree, 0));
  ck_assert(!praef_hash_tree_get_id(&object, tree, 1));
  ck_assert(!praef_hash_tree_get_id(&object, tree, ~0));
}

deftest(object_insertion_changes_dir_sids) {
  praef_hash_tree_objref object;
  const praef_hash_tree_directory* dir;
  praef_hash_tree_directory olddir;
  praef_hash_tree_cursor cursor = { { 0 }, 0 };
  unsigned i;

  for (i = 0; i < 256; ++i) {
    object.size = sizeof(i);
    object.instant = 0;
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
  }

  dir = praef_hash_tree_readdir(tree, &cursor);
  ck_assert_ptr_ne(NULL, dir);
  memcpy(&olddir, dir, sizeof(olddir));

  for (i = 256; i < 512; ++i) {
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
  }

  dir = praef_hash_tree_readdir(tree, &cursor);
  ck_assert_ptr_ne(NULL, dir);

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    ck_assert_int_eq(praef_htet_directory, dir->types[i]);
    ck_assert_int_eq(praef_htet_directory, olddir.types[i]);
    ck_assert_int_ne(olddir.sids[i], dir->sids[i]);
  }
}

deftest(fork_directories_unaffected_by_object_insertion) {
  const praef_hash_tree* fork;
  praef_hash_tree_objref object;
  const praef_hash_tree_directory* dir;
  praef_hash_tree_directory oldroot, oldsd;
  praef_hash_tree_cursor cursor = { { 0 }, 0 };
  unsigned i;

  for (i = 0; i < 256; ++i) {
    object.size = sizeof(i);
    object.instant = 0;
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
  }

  fork = praef_hash_tree_fork(tree);
  ck_assert_ptr_ne(NULL, fork);

  cursor.offset = 0;
  dir = praef_hash_tree_readdir(fork, &cursor);
  ck_assert_ptr_ne(NULL, dir);
  memcpy(&oldroot, dir, sizeof(oldroot));
  dir = praef_hash_tree_readdir(tree, &cursor);
  ck_assert_ptr_ne(NULL, dir);
  ck_assert(!memcmp(&oldroot, dir, sizeof(oldroot)));

  cursor.offset = 1;
  dir = praef_hash_tree_readdir(fork, &cursor);
  ck_assert_ptr_ne(NULL, dir);
  memcpy(&oldsd, dir, sizeof(oldsd));
  dir = praef_hash_tree_readdir(tree, &cursor);
  ck_assert_ptr_ne(NULL, dir);
  ck_assert(!memcmp(&oldsd, dir, sizeof(oldsd)));

  for (i = 256; i < 512; ++i) {
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
  }

  cursor.offset = 0;
  dir = praef_hash_tree_readdir(tree, &cursor);
  ck_assert_ptr_ne(NULL, dir);
  ck_assert(!!memcmp(&oldroot, dir, sizeof(oldroot)));
  dir = praef_hash_tree_readdir(fork, &cursor);
  ck_assert_ptr_ne(NULL, dir);
  ck_assert(!memcmp(&oldroot, dir, sizeof(oldroot)));

  cursor.offset = 1;
  dir = praef_hash_tree_readdir(tree, &cursor);
  ck_assert_ptr_ne(NULL, dir);
  ck_assert(!!memcmp(&oldsd, dir, sizeof(oldsd)));
  dir = praef_hash_tree_readdir(fork, &cursor);
  ck_assert_ptr_ne(NULL, dir);
  ck_assert(!memcmp(&oldsd, dir, sizeof(oldsd)));

  praef_hash_tree_delete(fork);
}

deftest(inserting_duplicate_object_has_no_effect) {
  praef_hash_tree_objref object;
  unsigned value = 42;

  object.instant = 0;
  object.size = sizeof(value);
  object.data = &value;
  ck_assert_int_eq(praef_htar_added, praef_hash_tree_add(tree, &object));

  /* Add a dupe, but with a different instant. When reading later, the old
   * instant should be reflected.
   */
  object.instant = 1;
  object.data = &value;
  ck_assert_int_eq(praef_htar_already_present,
                   praef_hash_tree_add(tree, &object));

  ck_assert(praef_hash_tree_get_id(&object, tree, 0));
  ck_assert_int_eq(0, object.instant);
}

deftest(can_get_object_by_hash_but_not_from_prior_fork) {
  const praef_hash_tree* fork;
  praef_hash_tree_objref object, result;
  unsigned i;
  unsigned char hash[PRAEF_HASH_SIZE];

  object.instant = 0;
  object.size = sizeof(i);
  for (i = 0; i < 256; ++i) {
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
  }

  fork = praef_hash_tree_fork(tree);
  ck_assert_ptr_ne(NULL, fork);

  i = 31337;
  object.data = &i;
  ck_assert(praef_hash_tree_add(tree, &object));
  praef_hash_tree_hash_of(hash, &object);

  ck_assert(praef_hash_tree_get_hash(&result, tree, hash));
  ck_assert(!memcmp(&object, &result, sizeof(object)));
  ck_assert_int_eq(31337, *(const int*)result.data);

  ck_assert(!praef_hash_tree_get_hash(&result, fork, hash));

  praef_hash_tree_delete(fork);
}

deftest(can_add_foreign_object_to_fork) {
  praef_hash_tree* fork;
  praef_hash_tree_objref object, result;
  unsigned i;
  unsigned char hash[PRAEF_HASH_SIZE];

  object.instant = 0;
  object.size = sizeof(i);
  for (i = 0; i < 256; ++i) {
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
  }

  fork = praef_hash_tree_fork(tree);
  ck_assert_ptr_ne(NULL, fork);

  i = 31337;
  object.data = &i;
  ck_assert(praef_hash_tree_add(tree, &object));
  ck_assert_int_eq(praef_htar_added,
                   praef_hash_tree_add_foreign(fork, object.id));
  praef_hash_tree_hash_of(hash, &object);

  ck_assert(praef_hash_tree_get_hash(&result, tree, hash));
  ck_assert(!memcmp(&object, &result, sizeof(object)));
  ck_assert_int_eq(31337, *(const int*)result.data);

  ck_assert(praef_hash_tree_get_hash(&result, fork, hash));
  ck_assert(!memcmp(&object, &result, sizeof(object)));
  ck_assert_int_eq(31337, *(const int*)result.data);

  praef_hash_tree_delete(fork);
}

deftest(equivalent_trees_produce_same_sids) {
  praef_hash_tree* other;
  const praef_hash_tree_directory* dira, * dirb;
  praef_hash_tree_objref object;
  praef_hash_tree_cursor cursor = { { 0 }, 0 };
  unsigned i, j;

  other = praef_hash_tree_new();
  ck_assert_ptr_ne(NULL, other);

  object.instant = 0;
  object.size = sizeof(i);
  for (i = 0; i < 256; ++i) {
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
    j = 255-i;
    object.data = &j;
    ck_assert(praef_hash_tree_add(other, &object));
  }

  dira = praef_hash_tree_readdir(tree, &cursor);
  dirb = praef_hash_tree_readdir(other, &cursor);
  ck_assert_ptr_ne(NULL, dira);
  ck_assert_ptr_ne(NULL, dirb);
  ck_assert_ptr_ne(dira, dirb);

  ck_assert(!memcmp(dira, dirb, sizeof(praef_hash_tree_directory)));

  praef_hash_tree_delete(other);
}

deftest(range_query_finds_exact_match) {
  praef_hash_tree_objref object;
  /* Allocating on the heap so valgrind can check it */
  praef_hash_tree_objref* dst;
  unsigned value = 42;
  unsigned char hash[PRAEF_HASH_SIZE];

  dst = malloc(sizeof(praef_hash_tree_objref));

  object.instant = 0;
  object.size = sizeof(value);
  object.data = &value;

  ck_assert(praef_hash_tree_add(tree, &object));
  praef_hash_tree_hash_of(hash, &object);
  ck_assert_int_eq(1, praef_hash_tree_get_range(dst, 1, tree, hash, 0, 0));
  ck_assert(!memcmp(&object, dst, sizeof(object)));

  free(dst);
}

deftest(range_query_finds_items_beyond_first) {
  praef_hash_tree_objref object;
  praef_hash_tree_objref* dst;
  unsigned i, nread;
  unsigned char hash[PRAEF_HASH_SIZE], hash2[PRAEF_HASH_SIZE];

  dst = calloc(256, sizeof(praef_hash_tree_objref));

  object.instant = 0;
  object.size = sizeof(i);

  for (i = 0; i < 256; ++i) {
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
  }

  /* Assumption: The last value added will not have the lexicographically last
   * hash value (only a 1/256 chance, even if the "hash" were random); no other
   * item shares the first (hash-size - 2) bytes with the final item
   * (unimaginably improbable); the final two bytes of the hash will not be
   * zero (1/65536 chance).
   */
  praef_hash_tree_hash_of(hash, &object);
  ck_assert(hash[PRAEF_HASH_SIZE-2]);
  ck_assert(hash[PRAEF_HASH_SIZE-1]);
  hash[PRAEF_HASH_SIZE-1] = 0;
  hash[PRAEF_HASH_SIZE-2] = 0;

  nread = praef_hash_tree_get_range(dst, 256, tree, hash, 0, 0);
  ck_assert_int_gt(nread, 1);
  ck_assert(!memcmp(&object, dst, sizeof(object)));

  for (i = 1; i < nread; ++i) {
    praef_hash_tree_hash_of(hash, dst+i-1);
    praef_hash_tree_hash_of(hash2, dst+i);
    ck_assert_int_lt(memcmp(hash, hash2, sizeof(hash)), 0);
  }

  free(dst);
}

deftest(range_query_filters_items_by_offset_and_mask) {
  praef_hash_tree_objref object;
  praef_hash_tree_objref* dst;
  unsigned i, nread;
  unsigned char hash[PRAEF_HASH_SIZE];

  dst = calloc(256, sizeof(praef_hash_tree_objref));

  object.instant = 0;
  object.size = sizeof(i);

  for (i = 0; i < 256; ++i) {
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
  }

  memset(hash, 0, sizeof(hash));
  nread = praef_hash_tree_get_range(dst, 256, tree, hash, 2, 0x3);
  ck_assert_int_gt(nread, 2);
  ck_assert_int_lt(nread, 255);

  for (i = 0; i < nread; ++i) {
    praef_hash_tree_hash_of(hash, dst + i);
    ck_assert_int_eq(2, hash[PRAEF_HASH_SIZE-1] & 0x3);
  }

  free(dst);
}

deftest(range_query_filters_honours_limit) {
  praef_hash_tree_objref object;
  praef_hash_tree_objref* dst;
  unsigned i, nread;
  unsigned char hash[PRAEF_HASH_SIZE];

  dst = calloc(1, sizeof(praef_hash_tree_objref));

  object.instant = 0;
  object.size = sizeof(i);

  for (i = 0; i < 256; ++i) {
    object.data = &i;
    ck_assert(praef_hash_tree_add(tree, &object));
  }

  memset(hash, 0, sizeof(hash));
  nread = praef_hash_tree_get_range(dst, 1, tree, hash, 0, 0);
  ck_assert_int_eq(1, nread);

  free(dst);
}

deftest(range_query_finds_nothing_for_last_hash) {
  praef_hash_tree_objref object;
  unsigned value = 42;
  unsigned char hash[PRAEF_HASH_SIZE];

  object.instant = 0;
  object.size = sizeof(value);
  object.data = &value;
  ck_assert(praef_hash_tree_add(tree, &object));

  memset(hash, ~0, sizeof(hash));
  ck_assert_int_eq(0, praef_hash_tree_get_range(NULL, 1, tree, hash, 0, 0));
}

deftest(range_query_finds_nothing_on_impossible_query) {
  praef_hash_tree_objref object;
  unsigned value = 42;
  unsigned char hash[PRAEF_HASH_SIZE];

  object.instant = 0;
  object.size = sizeof(value);
  object.data = &value;
  ck_assert(praef_hash_tree_add(tree, &object));

  memset(hash, 00, sizeof(hash));
  ck_assert_int_eq(0, praef_hash_tree_get_range(NULL, 1, tree, hash, 1, 0));
}
