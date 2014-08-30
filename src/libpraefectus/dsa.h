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
#ifndef LIBPRAEFECTUS_DSA_H_
#define LIBPRAEFECTUS_DSA_H_

#include <stdlib.h>

#include "dsa-parms.h"
#include "object.h"

/**
 * The number of bytes in a praefectus DSA signature.
 */
#define PRAEF_SIGNATURE_SIZE (PRAEF_DSA_N * 2 / 8)
/**
 * The number of bytes in a praefectus DSA public key.
 */
#define PRAEF_PUBKEY_SIZE (PRAEF_DSA_L / 8)

/**
 * The type for the public-key hint attached to high-level messages. When
 * converting to a byte sequence, this is to be encoded as a little-endian
 * integer.
 */
typedef unsigned short praef_pubkey_hint;

/**
 * The signator supports the signing of arbitrary data arrays, via the DSA
 * (though not exactly its official form).
 *
 * The DSA parameters are fixed across all praefectus instances, and can be
 * found in the dsa-parms.h. This obviates the need to include these rather
 * large parameters in the public key.
 *
 * Public keys are therefore simply the y field, which is encoded as a
 * little-endian 512-bit (64-byte) integer for transmission purposes.
 *
 * Signatures consist of two 128-bit integers (r,s), which are encoded as
 * little-endian integers for transmission purposes. This means that signatures
 * are only 32 bytes long.
 *
 * SHA-3 is used as the hash function, though using only the first 128 bits.
 *
 * Note that the N and L parameters are substantially smaller than what is
 * contemporarily recommended for DSA (the minimum specified by FIPS is
 * (1024,160), whereas here we have (512,128)). This decision was made based on
 * the belief that (a) the consequences of the signatures being broken are not
 * extremely severe, (b) the keys will not be meaningful for long enough for
 * them to be broken anyway; and (c) nobody would be willing to make the
 * investment in computing power to break them. Should these prove false for
 * some application, new values for the constants can be produced by tweaking
 * and rerunning gen-dsa-parms.c.
 *
 * In other words, performance and space-efficiency are preferred here over
 * decades worth of security.
 *
 * This implementation makes no attempt to conceal or securely destroy private
 * keys, for the same reasons outlined above.
 */
typedef struct praef_signator_s praef_signator;

/**
 * Creates a new signator with a unique public key.
 *
 * Note that this call creates GMP integers; the default GMP allocator aborts
 * the program if it runs out of memory. If the application wishes to handle
 * such cases, it must configure its own GMP allocators (ie, call
 * mp_set_memory_functions()).
 *
 * @return The new signator, or NULL if no signator could be obtained. This can
 * happen due to memory exhaustion or due to being unable to read from the
 * system's entropy source.
 */
praef_signator* praef_signator_new(void);
/**
 * Frees the memory held by the given signator.
 */
void praef_signator_delete(praef_signator*);

/**
 * Signs the given data array using this signator. This call always succeeds.
 *
 * @param signature Buffer which will hold the resulting signature.
 * @param data The data to be signed.
 * @param size The size of the data array.
 */
void praef_signator_sign(unsigned char signature[PRAEF_SIGNATURE_SIZE],
                         praef_signator*,
                         const void* data, size_t size);
/**
 * Copies the public key out of the given signator into the given destination
 * array.
 */
void praef_signator_pubkey(unsigned char key[PRAEF_PUBKEY_SIZE],
                           const praef_signator*);
/**
 * Returns the public-key hint to be used with this signator.
 */
praef_pubkey_hint praef_signator_pubkey_hint(const praef_signator*);

/**
 * A verifier can identify the originating node of a signature and verify the
 * validity of a signature produced by the signator.
 */
typedef struct praef_verifier_s praef_verifier;

/**
 * Allocates a new verifier with no associated nodes.
 *
 * Note that this call creates GMP integers; the default GMP allocator aborts
 * the program if it runs out of memory. If the application wishes to handle
 * such cases, it must configure its own GMP allocators (ie, call
 * mp_set_memory_functions()).
 *
 * @return The new verifier, or NULL if there is insufficient memory.
 */
praef_verifier* praef_verifier_new(void);
/**
 * Frees the memory held by the given verifier.
 */
void praef_verifier_delete(praef_verifier*);

/**
 * Associates a public-key/node-id pair in the given verifier, allowing it to
 * identify messages coming from that node.
 *
 * @param key The public key of the given node.
 * @param node_id The chosen local ID of the node. May not be zero.
 * @return Whether the association was made. The association will not be made
 * if memory is exhausted during the call, or if the public key is already
 * associated.
 */
int praef_verifier_assoc(praef_verifier*,
                         const unsigned char key[PRAEF_PUBKEY_SIZE],
                         praef_object_id node_id);
/**
 * Disassociates the given public key from the verifier.
 *
 * @param key The public key to disassociate.
 * @return Whether such a key was disassociated.
 */
int praef_verifier_disassoc(praef_verifier*,
                            const unsigned char key[PRAEF_PUBKEY_SIZE]);

/**
 * Returns whether the given public key has already been registered with the
 * given verifier.
 */
int praef_verifier_is_assoc(praef_verifier*,
                            const unsigned char key[PRAEF_PUBKEY_SIZE]);

/**
 * Attempts to identify the source node of a message and verify its signature.
 *
 * @param hint The public-key hint attached to the message.
 * @param sig The signature attached to the message.
 * @param data The message itself.
 * @param sz The size of the data array.
 * @return If successful (ie, a valid signature from a known node), the id of
 * the node that created the message (as given to praef_verifier_assoc()).
 * Otherwise, 0.
 */
praef_object_id praef_verifier_verify(
  praef_verifier*,
  praef_pubkey_hint hint,
  const unsigned char sig[PRAEF_SIGNATURE_SIZE],
  const void* data, size_t sz);

#endif /* LIBPRAEFECTUS_DSA_H_ */
