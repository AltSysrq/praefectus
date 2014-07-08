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
#include <stdlib.h>

#if defined(HAVE_GMP_H)
#include <gmp.h>
#elif defined(HAVE_MPIR_H)
#include <mpir.h>
#else
#error Neither gmp.h nor mpir.h appears to exist.
#endif

#include "common.h"
#include "secure-random.h"
#include "keccak.h"
#include "dsa-parms.h"
#include "dsa.h"

#define PRAEF_SIGINT_SIZE (PRAEF_DSA_N / 8)

struct praef_signator_s {
  /* As per standard DSA */
  mpz_t p, q, g, y, x, r, s, k;
  mpz_t kinv;
  /* The salt for generating k */
  unsigned char salt[PRAEF_SIGINT_SIZE];
  praef_pubkey_hint pubkey_hint;
};

static praef_pubkey_hint praef_calc_pubkey_hint(mpz_t);

praef_signator* praef_signator_new(void) {
  unsigned char xbuf[PRAEF_SIGINT_SIZE];
  praef_signator* this = malloc(sizeof(praef_signator));
  if (!this) return NULL;

  /* Initialise constants */
  mpz_inits(this->p, this->q, this->g,
            this->y, this->x, this->k, this->r, this->s,
            this->kinv, NULL);
  if (mpz_set_str(this->p, PRAEF_DSA_P, 16) ||
      mpz_set_str(this->q, PRAEF_DSA_Q, 16) ||
      mpz_set_str(this->g, PRAEF_DSA_G, 16)) {
    praef_signator_delete(this);
    return NULL;
  }

  /* Generate private key */
  do {
    if (!praef_secure_random(xbuf, sizeof(xbuf))) {
      praef_signator_delete(this);
      return NULL;
    }

    mpz_import(this->x, sizeof(xbuf), -1, 1, 0, 0, xbuf);
  } while (mpz_cmp_ui(this->x, 0) <= 0 ||
           mpz_cmp(this->x, this->q) >= 0);

  /* Use the raw private key as the k-generation salt */
  memcpy(this->salt, xbuf, sizeof(this->salt));

  /* Derive the public key */
  mpz_powm(this->y, this->g, this->x, this->p);
  this->pubkey_hint = praef_calc_pubkey_hint(this->y);

  return this;
}

static praef_pubkey_hint praef_calc_pubkey_hint(mpz_t pubkey) {
  unsigned char serialised[PRAEF_SIGINT_SIZE];
  unsigned char hash[sizeof(praef_pubkey_hint)];
  praef_keccak_sponge sponge;

  memset(serialised, 0, sizeof(serialised));
  mpz_export(serialised, NULL, -1, 1, 0, 0, pubkey);

  praef_keccak_sponge_init(&sponge, PRAEF_KECCAK_RATE, PRAEF_KECCAK_CAP);
  praef_keccak_sponge_absorb(&sponge, serialised, sizeof(serialised));
  praef_keccak_sponge_squeeze(&sponge, hash, sizeof(hash));

  return hash[0] | (((praef_pubkey_hint)hash[1]) << 8);
}

void praef_signator_delete(praef_signator* this) {
  mpz_clears(this->p, this->q, this->g,
             this->y, this->x, this->r, this->s, this->k,
             this->kinv, NULL);
  free(this);
}

void praef_signator_sign(unsigned char signature[PRAEF_SIGNATURE_SIZE],
                         praef_signator* this,
                         const void* data, size_t sz) {
  unsigned char hash[PRAEF_SIGINT_SIZE], kb[PRAEF_SIGINT_SIZE];
  praef_keccak_sponge sponge;

  /* Generate the base message hash */
  praef_keccak_sponge_init(&sponge, PRAEF_KECCAK_RATE, PRAEF_KECCAK_CAP);
  praef_keccak_sponge_absorb(&sponge, data, sz);
  praef_keccak_sponge_squeeze(&sponge, hash, sizeof(hash));

  /* Generate k. This is similar to RFC 6979, but more direct since we have
   * full access to the hash.
   */
  memcpy(kb, hash, sizeof(kb));

  produce_new_k:
  do {
    praef_keccak_sponge_init(&sponge, PRAEF_KECCAK_RATE, PRAEF_KECCAK_CAP);
    praef_keccak_sponge_absorb(&sponge, kb, sizeof(kb));
    praef_keccak_sponge_absorb(&sponge, this->salt, sizeof(this->salt));
    praef_keccak_sponge_squeeze(&sponge, kb, sizeof(kb));
    mpz_import(this->k, sizeof(kb), -1, 1, 0, 0, kb);
  } while (mpz_cmp_ui(this->k, 0) <= 0 ||
           mpz_cmp(this->k, this->q) >= 0);

  /* Calculate R */
  mpz_powm(this->r, this->g, this->k, this->p);
  mpz_mod(this->r, this->r, this->q);
  if (!mpz_cmp_ui(this->r, 0)) goto produce_new_k;

  /* Calculate S */
  mpz_import(this->s, PRAEF_SIGINT_SIZE, -1, 1, 0, 0, kb);
  mpz_addmul(this->s, this->x, this->r);
  mpz_invert(this->kinv, this->k, this->q);
  mpz_mul(this->s, this->kinv, this->s);
  mpz_mod(this->s, this->s, this->q);
  if (!mpz_cmp_ui(this->s, 0)) goto produce_new_k;

  /* GMP integer export only produces variable-length fields, stopping as soon
   * as a zero word would be written. Fortunately, with little-endian, this
   * means we just have to manually zero out the signature first; we don't even
   * care how many bytes are actually changed.
   */
  memset(signature, 0, PRAEF_SIGNATURE_SIZE);
  mpz_export(signature, NULL, -1, 1, 0, 0, this->r);
  mpz_export(signature + PRAEF_SIGINT_SIZE, NULL, -1, 1, 0, 0, this->s);
}

praef_pubkey_hint praef_signator_pubkey_hint(const praef_signator* this) {
  return this->pubkey_hint;
}

void praef_signator_pubkey(unsigned char key[PRAEF_PUBKEY_SIZE],
                           const praef_signator* this) {
  memset(key, 0, PRAEF_PUBKEY_SIZE);
  mpz_export(key, NULL, -1, 1, 0, 0, this->y);
}

typedef struct praef_verifier_entry_s {
  praef_pubkey_hint hint;
  mpz_t y;
  praef_object_id node_id;

  RB_ENTRY(praef_verifier_entry_s) tree;
} praef_verifier_entry;

static int praef_compare_verifier_entry(
  const praef_verifier_entry* a,
  const praef_verifier_entry* b
) {
  if (a->hint < b->hint) return -1;
  if (a->hint > b->hint) return +1;

  return mpz_cmp(a->y, b->y);
}

RB_HEAD(praef_verifier_entry_tree, praef_verifier_entry_s);
RB_PROTOTYPE(praef_verifier_entry_tree, praef_verifier_entry_s,
             tree, praef_compare_verifier_entry)
RB_GENERATE(praef_verifier_entry_tree, praef_verifier_entry_s,
            tree, praef_compare_verifier_entry)

struct praef_verifier_s {
  mpz_t p, q, g, r, s, w, u1, u2, v, h;
  praef_verifier_entry example;

  struct praef_verifier_entry_tree entries;
};

praef_verifier* praef_verifier_new(void) {
  praef_verifier* this = malloc(sizeof(praef_verifier));
  if (!this) return NULL;

  RB_INIT(&this->entries);

  mpz_inits(this->p, this->q, this->g,
            this->r, this->s, this->h,
            this->w, this->u1, this->u2, this->v,
            this->example.y, NULL);
  if (mpz_set_str(this->p, PRAEF_DSA_P, 16) ||
      mpz_set_str(this->q, PRAEF_DSA_Q, 16) ||
      mpz_set_str(this->g, PRAEF_DSA_G, 16)) {
    praef_verifier_delete(this);
    return NULL;
  }

  mpz_set_ui(this->example.y, 0);

  return this;
}

static void praef_verifier_entry_delete(praef_verifier_entry*);

void praef_verifier_delete(praef_verifier* this) {
  praef_verifier_entry* e, * tmp;

  for (e = RB_MIN(praef_verifier_entry_tree, &this->entries);
       e; e = tmp) {
    tmp = RB_NEXT(praef_verifier_entry_tree, &this->entries, e);
    RB_REMOVE(praef_verifier_entry_tree, &this->entries, e);
    praef_verifier_entry_delete(e);
  }

  mpz_clears(this->p, this->q, this->g,
             this->r, this->s, this->h,
             this->w, this->u1, this->u2, this->v,
             this->example.y, NULL);
  free(this);
}

static void praef_verifier_entry_init(
  praef_verifier_entry* this,
  const unsigned char pubkey[PRAEF_PUBKEY_SIZE],
  praef_object_id node_id
) {
  mpz_init(this->y);
  mpz_import(this->y, PRAEF_PUBKEY_SIZE, -1, 1, 0, 0, pubkey);
  this->node_id = node_id;
  this->hint = praef_calc_pubkey_hint(this->y);
}

static praef_verifier_entry* praef_verifier_entry_new(
  const unsigned char pubkey[PRAEF_PUBKEY_SIZE],
  praef_object_id node_id
) {
  praef_verifier_entry* this = malloc(sizeof(praef_verifier_entry));
  praef_verifier_entry_init(this, pubkey, node_id);
  return this;
}

static void praef_verifier_entry_clear(praef_verifier_entry* this) {
  mpz_clear(this->y);
}

static void praef_verifier_entry_delete(praef_verifier_entry* this) {
  praef_verifier_entry_clear(this);
  free(this);
}

int praef_verifier_assoc(praef_verifier* this,
                         const unsigned char key[PRAEF_PUBKEY_SIZE],
                         praef_object_id node_id) {
  praef_verifier_entry* entry;

  entry = praef_verifier_entry_new(key, node_id);
  if (!entry) return 0;

  if (RB_INSERT(praef_verifier_entry_tree, &this->entries, entry)) {
    /* Conflict */
    praef_verifier_entry_delete(entry);
    return 0;
  }

  return 1;
}

int praef_verifier_disassoc(praef_verifier* this,
                            const unsigned char key[PRAEF_PUBKEY_SIZE]) {
  praef_verifier_entry example, * removed;

  praef_verifier_entry_init(&example, key, 0);
  removed = RB_REMOVE(praef_verifier_entry_tree, &this->entries, &example);
  if (removed)
    praef_verifier_entry_delete(removed);

  praef_verifier_entry_clear(&example);
  return !!removed;
}

/**
 * Checks whether the given entry can verify a signature. It is assumed the
 * (h,r,s) variables of the verifier have been populated, that (u1,u2) have
 * been calculated. This uses the (w,v) temporaries of the verifier.
 *
 * It is assumed the 0<r<q && 0<s<q check has already been performed.
 */
static int praef_verifier_entry_verify(praef_verifier* this,
                                       praef_verifier_entry* entry) {
  /* w isn't needed anymore; reuse as temporary for g**u1 */
  mpz_powm(this->w, this->g, this->u1, this->p);
  mpz_powm(this->v, entry->y, this->u2, this->p);
  mpz_mul(this->v, this->v, this->w);
  mpz_mod(this->v, this->v, this->p);
  mpz_mod(this->v, this->v, this->q);
  return 0 == mpz_cmp(this->v, this->r);
}

praef_object_id praef_verifier_verify(
  praef_verifier* this,
  praef_pubkey_hint hint,
  const unsigned char sig[PRAEF_SIGNATURE_SIZE],
  const void* data, size_t sz
) {
  praef_verifier_entry* entry;
  unsigned char hash[PRAEF_SIGINT_SIZE];
  praef_keccak_sponge sponge;

  /* Find the first entry with the given pubkey hint. Fail fast if there is no
   * such entry.
   */
  this->example.hint = hint;
  entry = RB_NFIND(praef_verifier_entry_tree, &this->entries, &this->example);
  if (!entry || entry->hint != hint) return 0;

  /* Decode the signature */
  mpz_import(this->r, PRAEF_SIGINT_SIZE, -1, 1, 0, 0, sig);
  mpz_import(this->s, PRAEF_SIGINT_SIZE, -1, 1, 0, 0, sig + PRAEF_SIGINT_SIZE);

  /* Sanity check the signature */
  if (mpz_cmp_ui(this->r, 0) <= 0 ||
      mpz_cmp_ui(this->s, 0) <= 0 ||
      mpz_cmp(this->r, this->q) >= 0 ||
      mpz_cmp(this->s, this->q) >= 0)
    return 0;

  /* Hash the message and read into an integer */
  praef_keccak_sponge_init(&sponge, PRAEF_KECCAK_RATE, PRAEF_KECCAK_CAP);
  praef_keccak_sponge_absorb(&sponge, data, sz);
  praef_keccak_sponge_squeeze(&sponge, hash, PRAEF_SIGINT_SIZE);
  mpz_import(this->h, PRAEF_SIGINT_SIZE, -1, 1, 0, 0, hash);

  /* Calculate the fields that do not depend upon the public key */
  mpz_invert(this->w, this->s, this->q);
  mpz_mul(this->u1, this->h, this->w);
  mpz_mod(this->u1, this->u1, this->q);
  mpz_mul(this->u2, this->r, this->w);
  mpz_mod(this->u2, this->u2, this->q);

  /* See if any of the entries have a key that can verify this signature */
  while (entry && entry->hint == hint) {
    if (praef_verifier_entry_verify(this, entry))
      return entry->node_id;

    entry = RB_NEXT(praef_verifier_entry_tree, &this->entries, entry);
  }

  /* No node has a public key that was used to sign this message (or someone is
   * lying about the hint).
   */
  return 0;
}
