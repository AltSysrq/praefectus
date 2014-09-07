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
#ifndef LIBPRAEFECTUS_KECCAK_H_
#define LIBPRAEFECTUS_KECCAK_H_

/* Convenience header to include the keccak interface correctly. */

#include "keccak-rename.h"
#include "keccak/KeccakSponge.h"

#define PRAEF_KECCAK_CAP (512)
#define PRAEF_KECCAK_RATE (1600 - PRAEF_KECCAK_CAP)

static inline void praef_sha3_init(praef_keccak_sponge* sponge) {
  praef_keccak_sponge_init(sponge, PRAEF_KECCAK_RATE, PRAEF_KECCAK_CAP);
}

static inline void praef_keccak_sponge_absorb_integer(
  praef_keccak_sponge* sponge,
  unsigned long long value,
  unsigned char nbytes
) {
  unsigned char bytes[nbytes];
  unsigned i;

  for (i = 0; i < nbytes; ++i)
    bytes[i] = (value >> 8*i) & 0xFF;

  praef_keccak_sponge_absorb(sponge, bytes, nbytes);
}

static inline unsigned long long praef_keccak_sponge_squeeze_integer(
  praef_keccak_sponge* sponge,
  unsigned char nbytes
) {
  unsigned char bytes[nbytes];
  unsigned i;
  unsigned long long val;

  praef_keccak_sponge_squeeze(sponge, bytes, nbytes);

  val = 0;
  for (i = 0; i < nbytes; ++i)
    val |= ((unsigned long long)bytes[i]) << i*8;

  return val;
}

#endif /* LIBPRAEFECTUS_KECCAK_H_ */
