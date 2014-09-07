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

void praef_node_recv_msg_htls(praef_node* node,
                              const PraefMsgHtLs_t* msg) {
  PraefMsg_t response;
  praef_hash_tree_cursor cursor;
  const praef_hash_tree_directory* dir;
  unsigned i;
  PraefHtdirEntry_t entries[PRAEF_HTDIR_SIZE],
                  * entries_ptrs[PRAEF_HTDIR_SIZE];

  if (!node->sys->htm.snapshots) return;

  memset(&response, 0, sizeof(response));

  praef_system_htm_create_cursor(&cursor, msg);
  dir = praef_hash_tree_readdir(
    praef_system_htm_get_snapshot(node->sys, msg->snapshot), &cursor);
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
      entries[i].choice.subdirsid = dir->sids[i];
      break;
    }
  }

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

void praef_node_rec_msg_htdir(praef_node* node,
                              const PraefMsgHtDir_t* msg) {
  praef_hash_tree_cursor cursor;
  const praef_hash_tree_directory* sn_dir, * curr_dir;
  praef_hash_tree_directory empty_dir;
  unsigned i;

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
  sn_dir = praef_system_htm_get_dir_or_empty(
    praef_system_htm_get_snapshot(node->sys, msg->request.snapshot),
    &cursor, &empty_dir);

  for (i = 0; i < PRAEF_HTDIR_SIZE; ++i) {
    switch (msg->entries.list.array[i]->present) {
    case PraefHtdirEntry_PR_NOTHING: abort();
    case PraefHtdirEntry_PR_empty: continue;
    case PraefHtdirEntry_PR_objectid:
      /* TODO: Right now there's no way to tell whether two objects that happen
       * to be colocated in two hash trees are actually the same, or just
       * happen to have the same prefix. We need to include some second-order
       * hash or something (probably 16- or 24-bit) to have high confidence in
       * them being different. Alternatively, we could provide the second-order
       * hash of all objects in directly in a directory, and indiscriminately
       * request all of them if the local and remote hashes differ, but all
       * object slots match.
       *
       * The second-order hash doesn't need to be terribly resistent to
       * birthday attacks, since matching the hash prefix causes the time
       * required to find collisions to grow exponentially with the length of
       * the prefix, and early on when the prefix is short, prefix collisions
       * will get resolved anyway as other messages experience prefix
       * collisions within the same tree and trigger the production of
       * subdirectories.
       *
       * Just abort for now until we fix this.
       */
      abort();

    case PraefHtdirEntry_PR_subdirsid:
      /* Recurse if neither the snapshot nor current agree with the directory
       * entry.
       */
      if ((praef_htet_directory != sn_dir->types[i] ||
           msg->entries.list.array[i]->choice.subdirsid != sn_dir->sids[i]) &&
          (praef_htet_directory != curr_dir->types[i] ||
           msg->entries.list.array[i]->choice.subdirsid != curr_dir->sids[i]))
        praef_node_htm_request_htls(node, msg->request.hash.buf,
                                    msg->request.hash.size,
                                    msg->request.lownybble, i);
      break;
    }
  }
}

static void praef_node_htm_request_htls(praef_node* node,
                                        const void* bytes,
                                        unsigned nbytes,
                                        int lownybble,
                                        unsigned subdir) {
  /* TODO */
  abort();
}
