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
#ifndef LIBPRAEFECTUS_HASH_TREE_H_
#define LIBPRAEFECTUS_HASH_TREE_H_

#include <stdlib.h>

#include "common.h"

__BEGIN_DECLS

/**
 * The number of entries in a hash tree directory.
 */
#define PRAEF_HTDIR_SIZE 16

/**
 * A hash tree (basically a Merkle tree, but with some extra functionality) is
 * a content-addressable byte-array store. Messages can be located based on
 * their content in logarithmic time (including range queries), and
 * discrepencies between two hash trees can be discovered in logarithmic time
 * when there are few differences.
 *
 * A hash tree is organised into a tree of directories and objects. The
 * top-level is always a directory. Each directory has a nybble-offset within
 * the hash; all objects directly or indirectly contained there in are
 * guaranteed to have identical hash codes through the first offset nybbles of
 * the hash code (running from byte 0 to byte 31, high nybble before low
 * nybble).
 *
 * Furthermore, each directory maintains a small hash derived from the hashes
 * of the objects and directories it contains. This permits the comparison of
 * two directories to determine whether they have equivalent contents.
 *
 * Objects, besides their hashes, are also associated with IDs unique (but also
 * particular) to the hash tree to permit constant-time access. These IDs are
 * not consistent across nodes, but are stable over the life of the hash tree.
 *
 * This hash tree implementation is semi-persistent. A copy of a hash tree can
 * be created at any time with constant cost. The directory structure of such a
 * copy is unaffected by modifications to the original. However, all copies
 * share the same object table, and thus will use the same id space for new
 * objects. It is also possible to locate any object by id from any of the
 * connected hash trees, even if the object is not actually present in the one
 * used for reference.
 */
typedef struct praef_hash_tree_s praef_hash_tree;

/**
 * The short-id/hash type used to identify directory entries.
 */
typedef unsigned long long praef_hash_tree_sid;

/**
 * The possible types for a hash tree directory entry. A false value is
 * guaranteed to indicate the absence of an entry, and a true value the
 * presence of one.
 */
typedef enum {
  /**
   * Indicates that the hash tree directory has no item in this field. The sid
   * is meaningless.
   */
  praef_htet_none = 0,
  /**
   * Indicates that the hash tree directory has an object in this field. The
   * sid is the object's monotonic ID, which is guaranteed to fit into the
   * lower 32 bits of the sid.
   */
  praef_htet_object,
  /**
   * Indicates t hat the hash tree directory has a subdirectory in this
   * field. The sid is the directory's short hash.
   */
  praef_htet_directory
} praef_hash_tree_entry_type;

/**
 * A directory inside a hash tree. The two arrays are indexed according to the
 * first nybble in the object hash after the directory's offset.
 */
typedef struct {
  /**
   * The types of the entries in this directory.
   */
  praef_hash_tree_entry_type types[PRAEF_HTDIR_SIZE];
  /**
   * The sids for the entries in this directory.
   */
  praef_hash_tree_sid sids[PRAEF_HTDIR_SIZE];
} praef_hash_tree_directory;

/**
 * A reference to data that may be in a hash tree.
 */
typedef struct {
  /**
   * The number of bytes in the data field.
   */
  unsigned size;
  /**
   * An instant associated with this object. The hash tree itself ascribes no
   * meaning to this field, but merely preserves it along with the object. It
   * has no effect on the calculated hash value.
   */
  praef_instant instant;
  /**
   * The id of the object within the hash tree.
   */
  praef_hash_tree_sid id;
  /**
   * The data associated with this object. If this points to the data within a
   * hash tree, it is guaranteed to be valid until the hash tree's destruction,
   * and there is guaranteed to be an initialised 0 byte at data[size].
   */
  const void* data;
} praef_hash_tree_objref;

/**
 * Describes the location of a directory within a hash tree.
 */
typedef struct {
  /**
   * The hash to use. Only the first offset nybbles are meaningful.
   */
  unsigned char hash[PRAEF_HASH_SIZE];
  /**
   * The number of nybbles of the hash to use for comparison. (Alternatively,
   * the depth of directory to obtain.)
   */
  unsigned offset;
} praef_hash_tree_cursor;

/**
 * Return type from praef_hash_tree_add().
 */
typedef enum {
  /**
   * Indicates that the operation failed (eg, out of memory). This is
   * guaranteed to be false.
   */
  praef_htar_failed = 0,
  /**
   * Indicates that a new object was added to the tree.
   */
  praef_htar_added,
  /**
   * Indicates that no new object was added to the tree because that object was
   * already present in the tree.
   */
  praef_htar_already_present
} praef_hash_tree_add_result;

/**
 * Allocates a new, empty, read-write hash tree.
 *
 * @return The hash tree, or NULL if memory could not be obtained.
 */
praef_hash_tree* praef_hash_tree_new(void);
/**
 * Creates a new copy of the given hash tree. Changes to directories in the
 * original will not be reflected in the copy. The copy remains valid even
 * after the original is destroyed.
 *
 * Complexity: O(1)
 *
 * @return The new hash tree, or NULL if memory could not be obtained.
 */
praef_hash_tree* praef_hash_tree_fork(const praef_hash_tree*);
/**
 * Releases the memory being held by the given hash tree alone.
 */
void praef_hash_tree_delete(const praef_hash_tree*);

/**
 * Adds an object to the given read-write hash tree. If no such event exists,
 * the given reference is copied into the tree; otherwise, no action is
 * taken. If successful, the given reference's data pointer is updated to point
 * to the tree's copy, permitting it to be used with, eg,
 * praef_hash_tree_hash_of(), and the id field is set to the id of the new
 * object.
 *
 * Complexity: O(log(n))
 *
 * @return praef_htar_failed (0) if the operation fails; praef_htar_added if
 * the item was inserted; praef_htar_already_present if the object was already
 * present in the hash tree.
 */
praef_hash_tree_add_result praef_hash_tree_add(
  praef_hash_tree*, praef_hash_tree_objref*);

/**
 * Structurally adds the object with the given id, which must represent an
 * object in the table shared between this hash tree and its
 * praef_hash_tree_fork()-derived relatives.
 *
 * Complexity: O(lon(n))
 *
 * @return praef_htar_failed (0) if the operation fails; praef_htar_added if
 * the item was inserted; praef_htar_already_present if the object was already
 * present in the hash tree.
 */
praef_hash_tree_add_result praef_hash_tree_add_foreign(
  praef_hash_tree*, praef_hash_tree_sid);

/**
 * Obtains an object reference by exact hash code. The contents of the given
 * objref are set to the data stored within the hash tree. On failure, the
 * output buffer is untouched.
 *
 * When possible, praef_hash_tree_get_id() is preferred.
 *
 * Complexity: O(log(n))
 *
 * @return Whether an object was read.
 */
int praef_hash_tree_get_hash(praef_hash_tree_objref*, const praef_hash_tree*,
                             const unsigned char hash[PRAEF_HASH_SIZE]);
/**
 * Obtains an object reference via its local ID. The contents of the given
 * objref are set to the data stored within the hash tree. On failure, the
 * object buffer is untouched.
 *
 * Complexity: O(1)
 *
 * @return Whether an object was read.
 */
int praef_hash_tree_get_id(praef_hash_tree_objref*, const praef_hash_tree*,
                           praef_hash_tree_sid);
/**
 * Obtains zero or more objects via base+offset query against their order
 * derived from their hash codes.
 *
 * Complexity: O(log(n) + count)
 *
 * @param dst The buffer into which to write references. This must be an array
 * of references at least count in length. The function will only modify as
 * many entries as its return value.
 * @param count The maximum number of objects to read.
 * @param tree The tree from which to read.
 * @param hash The minimum hash code of any object to read.
 * @param offset Only objects where (hash[31]&mask) equals this value will be
 * found by this call.
 * @param mask Controls the frequency at which events are found (see offset).
 * @return The number of objects this call found.
 */
unsigned praef_hash_tree_get_range(praef_hash_tree_objref* dst, unsigned count,
                                   const praef_hash_tree* tree,
                                   const unsigned char hash[PRAEF_HASH_SIZE],
                                   unsigned char offset, unsigned char mask);

/**
 * Returns a directory entry within the given hash tree corresponding to the
 * given cursor, or NULL if there is no such directory.
 *
 * Complexity: O(offset)
 *
 * The returned value is only guaranteed to be valid until the next
 * modification of the hash tree.
 */
const praef_hash_tree_directory*
praef_hash_tree_readdir(const praef_hash_tree*, const praef_hash_tree_cursor*);

/**
 * Returns the minimum number of nybbles required from the given hash to
 * uniquely identify the final directory or object a tree traversal
 * encounters.
 */
unsigned praef_hash_tree_minimum_hash_length(
  const praef_hash_tree*, const unsigned char hash[PRAEF_HASH_SIZE]);

/**
 * Obtains the hash value of an object within a hash tree. The effect of this
 * call is undefined if the objref is not present in a hash tree (eg, if the
 * caller constructed it itself and didn't pass it to a successful call to
 * praef_hash_tree_add() first).
 *
 * Complexity: O(1)
 *
 * @param dst Buffer to fill with the hash value.
 */
void praef_hash_tree_hash_of(unsigned char dst[PRAEF_HASH_SIZE],
                             const praef_hash_tree_objref*);

/**
 * Returns a pointer to the hash of the object with the given id. The effect of
 * this call is undefined if the objref is not present in hash tree. The
 * returned pointer is only valid until the next modification to the hash
 * tree.
 */
const unsigned char* praef_hash_tree_get_hash_of(
  const praef_hash_tree_objref*);

__END_DECLS

#endif /* LIBPRAEFECTUS_HASH_TREE_H_ */
