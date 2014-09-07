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

#include "messages/PraefMsg.h"

#include "keccak.h"
#include "system.h"
#include "-system.h"

static int praef_node_htm_is_visible(praef_node*, praef_instant);
static const praef_hash_tree* praef_system_htm_get_snapshot(
  praef_system*, praef_instant);
static void praef_system_htm_create_cursor(
  praef_hash_tree_cursor* dst,
  const PraefMsgHtLs_t*);
static const praef_hash_tree_directory* praef_system_htm_get_dir_or_empty(
  const praef_hash_tree*, const praef_hash_tree_cursor*,
  const praef_hash_tree_directory* empty);
static void praef_node_htm_request_htls(
  praef_node*, const void* prefix, unsigned size,
  int lownybble, unsigned subdir);
static void praef_node_htm_request_htread(
  praef_node*, PraefDword_t);
static PraefDword_t praef_system_htm_objhash(
  const praef_hash_tree*,
  const praef_hash_tree_directory* dir,
  const unsigned char pubkey[PRAEF_PUBKEY_SIZE],
  praef_instant);
static PraefDword_t praef_system_htm_dirhash(
  praef_hash_tree_sid sid,
  const unsigned char pubkey[PRAEF_PUBKEY_SIZE],
  praef_instant);

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

static int praef_node_htm_is_visible(praef_node* node,
                                     praef_instant instant) {
  /* TODO */
  return 1;
}

static const praef_hash_tree* praef_system_htm_get_snapshot(
  praef_system* sys, praef_instant instant
) {
  unsigned i;

  for (i = 0; i < sys->htm.num_snapshots; ++i)
    if (sys->htm.snapshots[i].tree &&
        sys->htm.snapshots[i].instant <= instant)
      return sys->htm.snapshots[i].tree;

  return sys->state.hash_tree;
}

static void praef_system_htm_create_cursor(
  praef_hash_tree_cursor* cursor,
  const PraefMsgHtLs_t* msg
) {
  memcpy(cursor->hash, msg->hash.buf, msg->hash.size);
  cursor->offset = 2 * msg->hash.size;
  if (cursor->offset && !msg->lownybble)
    --cursor->offset;
}

void praef_node_htm_recv_msg_htls(praef_node* node,
                                  const PraefMsgHtLs_t* msg) {
  PraefMsg_t response;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE],
                  * entries_ptrs[PRAEF_HTDIR_SIZE];
  const praef_hash_tree* tree;
  praef_hash_tree_cursor cursor;
  const praef_hash_tree_directory* dir;
  unsigned i;

  if (!node->sys->htm.snapshots) return;

  memset(&response, 0, sizeof(response));

  praef_system_htm_create_cursor(&cursor, msg);
  tree = praef_system_htm_get_snapshot(node->sys, msg->snapshot);
  dir = praef_hash_tree_readdir(tree, &cursor);
  if (!dir) return;

  response.choice.htdir.entries.list.array = entries_ptrs;
  response.choice.htdir.entries.list.count = PRAEF_HTDIR_SIZE;
  response.choice.htdir.entries.list.size = PRAEF_HTDIR_SIZE;

  response.present = PraefMsg_PR_htdir;
  memcpy(&response.choice.htdir.request, msg, sizeof(PraefMsgHtLs_t));

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    entries_ptrs[i] = entries+i;

    switch (dir->types[i]) {
    case praef_htet_none:
      entries[i].present = PraefHtdirEntry_PR_empty;
      break;

    case praef_htet_object:
      entries[i].present = PraefHtdirEntry_PR_objectid;
      entries[i].choice.objectid = dir->sids[i];
      break;

    case praef_htet_directory:
      entries[i].present = PraefHtdirEntry_PR_subdirsid;
      entries[i].choice.subdirsid = praef_system_htm_dirhash(
        dir->sids[i], node->pubkey, msg->snapshot);
      break;
    }
  }

  response.choice.htdir.objhash = praef_system_htm_objhash(
    tree, dir, node->pubkey, msg->snapshot);

  PRAEF_OOM_IF_NOT(node->sys, praef_outbox_append(
                     node->router.rpc_out, &response));
}

static const praef_hash_tree_directory* praef_system_htm_get_dir_or_empty(
  const praef_hash_tree* tree, const praef_hash_tree_cursor* cursor,
  const praef_hash_tree_directory* empty
) {
  const praef_hash_tree_directory* dir;

  dir = praef_hash_tree_readdir(tree, cursor);
  return dir? dir : empty;
}

static PraefDword_t praef_system_htm_objhash(
  const praef_hash_tree* tree,
  const praef_hash_tree_directory* dir,
  const unsigned char pubkey[PRAEF_PUBKEY_SIZE],
  praef_instant instant
) {
  praef_keccak_sponge sponge;
  praef_hash_tree_objref object;
  unsigned i;

  praef_sha3_init(&sponge);

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    if (praef_htet_object == dir->types[i]) {
      praef_hash_tree_get_id(&object, tree, dir->sids[i]);
      praef_keccak_sponge_absorb(
        &sponge, praef_hash_tree_get_hash_of(&object),
        PRAEF_HASH_SIZE);
    }
  }

  praef_keccak_sponge_absorb(&sponge, pubkey, PRAEF_PUBKEY_SIZE);
  praef_keccak_sponge_absorb_integer(&sponge, instant, sizeof(praef_instant));
  return praef_keccak_sponge_squeeze_integer(&sponge, sizeof(PraefDword_t));
}

static PraefDword_t praef_system_htm_dirhash(
  praef_hash_tree_sid sid,
  const unsigned char pubkey[PRAEF_PUBKEY_SIZE],
  praef_instant instant
) {
  praef_keccak_sponge sponge;

  praef_sha3_init(&sponge);
  praef_keccak_sponge_absorb_integer(&sponge, sid, sizeof(sid));
  praef_keccak_sponge_absorb(&sponge, pubkey, PRAEF_PUBKEY_SIZE);
  praef_keccak_sponge_absorb_integer(&sponge, instant, sizeof(instant));
  return praef_keccak_sponge_squeeze_integer(&sponge, sizeof(PraefDword_t));
}

void praef_node_htm_recv_msg_htdir(praef_node* node,
                                   const PraefMsgHtDir_t* msg) {
  praef_hash_tree_cursor cursor;
  const praef_hash_tree* sn_tree;
  const praef_hash_tree_directory* sn_dir, * curr_dir;
  praef_hash_tree_directory empty_dir;
  unsigned i;
  /* Track whether any differences were found comparing both local trees and
   * the remote directory. If not, we need to check the objhash for the
   * directory to determine whether there are any discrepencies in the
   * objects.
   */
  int any_differences_found = 0;

  if (!node->sys->local_node) return;

  memset(&empty_dir, 0, sizeof(empty_dir));

  praef_system_htm_create_cursor(&cursor, &msg->request);
  /* Get both the current version of the directory and the snapshot version. We
   * use the current version to see whether any new objects are visible and the
   * both versions to decide whether to recurse into subdirectories.
   *
   * If a directory is absent, just assume it is empty locally (which is
   * logically correct).
   */
  curr_dir = praef_system_htm_get_dir_or_empty(
    node->sys->state.hash_tree, &cursor, &empty_dir);
  sn_tree = praef_system_htm_get_snapshot(node->sys, msg->request.snapshot);
  sn_dir = praef_system_htm_get_dir_or_empty(
    sn_tree, &cursor, &empty_dir);

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    switch (msg->entries.list.array[i]->present) {
    case PraefHtdirEntry_PR_NOTHING: abort();
    case PraefHtdirEntry_PR_empty:
      any_differences_found |=
        (praef_htet_object == curr_dir->types[i] ||
         praef_htet_object == sn_dir->types[i]);
      break;

    case PraefHtdirEntry_PR_objectid:
      /* Here, we can only identify whether or not the local and remote
       * directories happen to have an object in this slot. At this point,
       * assume that they are the same objects if they both happen to be
       * present (though they could just share a prefix). If no differences in
       * objects are found at the end of the loop, we'll need to compare the
       * objhashes on the directories and, if they are different,
       * pessimistically request all the objects.
       */
      if (praef_htet_object != curr_dir->types[i] &&
          praef_htet_object != sn_dir->types[i]) {
        any_differences_found = 1;
        praef_node_htm_request_htread(
          node, msg->entries.list.array[i]->choice.objectid);
      }
      break;

    case PraefHtdirEntry_PR_subdirsid:
      any_differences_found |=
        (praef_htet_object == curr_dir->types[i] ||
         praef_htet_object == sn_dir->types[i]);

      /* Recurse if neither the snapshot nor current agree with the directory
       * entry.
       */
      if ((praef_htet_directory != sn_dir->types[i] ||
           msg->entries.list.array[i]->choice.subdirsid !=
           praef_system_htm_dirhash(
             sn_dir->sids[i], node->sys->local_node->pubkey,
             msg->request.snapshot)) &&
          (praef_htet_directory != curr_dir->types[i] ||
           msg->entries.list.array[i]->choice.subdirsid !=
           praef_system_htm_dirhash(
             curr_dir->sids[i], node->sys->local_node->pubkey,
             msg->request.snapshot)))
        praef_node_htm_request_htls(node, msg->request.hash.buf,
                                    msg->request.hash.size,
                                    msg->request.lownybble, i);
      break;
    }
  }

  if (!any_differences_found) {
    /* No differences found between the object slots. Check whether the object
     * contents are actually the same. If not, request all the objects.
     */
    if (msg->objhash != praef_system_htm_objhash(node->sys->state.hash_tree,
                                                 curr_dir,
                                                 node->sys->local_node->pubkey,
                                                 msg->request.snapshot) &&
        msg->objhash != praef_system_htm_objhash(sn_tree, sn_dir,
                                                 node->sys->local_node->pubkey,
                                                 msg->request.snapshot)) {
      for (i = 0; i < PRAEF_HTDIR_SIZE; ++i)
        if (PraefHtdirEntry_PR_objectid ==
            msg->entries.list.array[i]->present)
          praef_node_htm_request_htread(
            node, msg->entries.list.array[i]->choice.objectid);
    }
  }
}

static void praef_node_htm_request_htls(praef_node* node,
                                        const void* bytes,
                                        unsigned nbytes,
                                        int lownybble,
                                        unsigned subdir) {
  PraefMsg_t request;
  unsigned char hash[PRAEF_HASH_SIZE];
  praef_instant instant;

  /* Can't request anything if at the bottom of the tree */
  if (nbytes >= PRAEF_HASH_SIZE && lownybble) return;

  if (node->sys->clock.monotime < node->sys->htm.root_query_offset)
    instant = 0;
  else
    instant = node->sys->clock.monotime - node->sys->htm.root_query_offset;

  memcpy(hash, bytes, nbytes);
  if (lownybble || !nbytes) {
    hash[nbytes] = subdir << 4;
    ++nbytes;
    lownybble = 0;
  } else {
    hash[nbytes-1] &= 0xF0;
    hash[nbytes-1] |= subdir;
    lownybble = 1;
  }

  memset(&request, 0, sizeof(request));
  request.present = PraefMsg_PR_htls;
  request.choice.htls.snapshot = instant
    / node->sys->htm.snapshot_interval
    * node->sys->htm.snapshot_interval;
  request.choice.htls.hash.buf = hash;
  request.choice.htls.hash.size = nbytes;
  request.choice.htls.lownybble = lownybble;

  PRAEF_OOM_IF_NOT(node->sys, praef_outbox_append(
                     node->router.rpc_out, &request));
}

static void praef_node_htm_request_htread(praef_node* node,
                                          PraefDword_t sid) {
  PraefMsg_t request;

  memset(&request, 0, sizeof(request));
  request.present = PraefMsg_PR_htread;
  request.choice.htread.objectid = sid;
  PRAEF_OOM_IF_NOT(node->sys, praef_outbox_append(
                     node->router.rpc_out, &request));
}
