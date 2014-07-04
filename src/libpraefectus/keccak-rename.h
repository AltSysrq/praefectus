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
#ifndef LIBPRAEFECTUS_KECCAK_RENAME_H_
#define LIBPRAEFECTUS_KECCAK_RENAME_H_

/* This file [ab]uses the preprocessor to rename the exported symbols from the
 * Keccak Code Package so we don't collide with other libraries or
 * applications. This is done with the preprocessor to minimise the edits that
 * must be made to the sources themselves.
 *
 * The new names are largely similar to the originals, but use libpraefectus's
 * scheme. StateInitialize is reduced to just init. Keccak_SpongeInstance is
 * renamed to praef_keccak_sponge for consistency, even though such a rename is
 * not strictly necessary.
 */

/* Since we only use the opt64 implementation, this function never needs to be
 * called. Rename it anyway for the sake of collision avoidance.
 */
#define KeccakF1600_Initialize praef_keccak_initialize_
#define KeccakF1600_StateInitialize praef_keccak_init
#define KeccakF1600_StateXORBytesInLane praef_keccak_state_xor_bytes_in_lane
#define KeccakF1600_StateXORLanes praef_keccak_state_xor_lanes
#define KeccakF1600_StateComplementBit praef_keccak_state_complement_bit
#define KeccakF1600_StatePermute praef_keccak_state_permute
#define KeccakF1600_StateExtractBytesInLane praef_keccak_state_extract_bytes_in_lane
#define KeccakF1600_StateExtractLanes praef_keccak_state_extract_lanes
#define KeccakF1600_StateXORPermuteExtract praef_keccak_state_xor_permute_extract
#define Keccak_SpongeInstance praef_keccak_sponge
#define Keccak_SpongeInitialize praef_keccak_sponge_init
#define Keccak_SpongeAbsorb praef_keccak_sponge_absorb
#define Keccak_SpongeAbsorbLastFewBits praef_keccak_sponge_absorb_last_few_bits
#define Keccak_SpongeSqueeze praef_keccak_sponge_squeeze

#endif /* LIBPRAEFECTUS_KECCAK_RENAME_H_ */
