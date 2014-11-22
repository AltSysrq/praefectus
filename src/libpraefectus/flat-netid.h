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
#ifndef LIBPRAEFECTUS_FLAT_NETID_H_
#define LIBPRAEFECTUS_FLAT_NETID_H_

__BEGIN_DECLS

#include "messages/PraefNetworkIdentifierPair.h"

/**
 * A flat_netid is a flattened (ie, single-memory-region) representation of a
 * PraefNetworkIdentifierPair_t, making it much easier to handle. An ASN.1
 * value can be assigned to a flat_netid via praef_flat_netid_from_asn1(), and
 * the converse  via praef_flat_netid_to_asn1().
 *
 * It is meaningful to assign one flat_netid to another.
 *
 * This structure should be treated by external code as essentially opaque.
 */
typedef struct {
  /**
   * The ASN.1 identifier pair structure.
   *
   * Do not access this directly; it's internal pointers are not guaranteed to
   * be meaningful.
   */
  PraefNetworkIdentifierPair_t backing_asn1;
  /**
   * The ASN.1 internet identifier structure.
   *
   * Do not access this directly; it's internal pointers are not guaranteed to
   * be meaningful.
   */
  PraefNetworkIdentifier_t backing_asn1inet;
  /**
   * Byte buffers backing the IP addresses.
   */
  unsigned char intranet[32], internet[32];
} praef_flat_netid;

/**
 * Copies the information from src into dst, such that no references into src
 * are required after the call returns.
 *
 * praef_flat_netid_to_asn1() can be called on dst after this call to produce
 * a logically equivalent id pair to src.
 */
void praef_flat_netid_from_asn1(praef_flat_netid* dst,
                                const PraefNetworkIdentifierPair_t* src);
/**
 * Produces a PraefNetworkIdentifierPair_t entirely backed by the given
 * flat_netid.
 *
 * The returned value becomes invalid if the flat_netid is destroyed, or if it
 * is mutated in any way (be it through praef_flat_netid_from_asn1() or a
 * direct assignment from another flat_netid).
 *
 * @param id The id to convert. Note that this is only _logically_ const; the
 * call may update internal pointers in order to produce the desired object.
 */
const PraefNetworkIdentifierPair_t*
praef_flat_netid_to_asn1(const praef_flat_netid* id);

__END_DECLS

#endif /* LIBPRAEFECTUS_FLAT_NETID_H_ */
