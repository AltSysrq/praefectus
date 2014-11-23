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

#include "flat-netid.h"

void praef_flat_netid_from_asn1(praef_flat_netid* dst,
                                const PraefNetworkIdentifierPair_t* src) {
  /* Don't bother adjusting the pointers here; in any case, this needs to be
   * done on each call to praef_flat_netid_to_asn1() to account for
   * assignments, and leaving the pointers uninitialised will better expose
   * incorrect usages.
   */
  dst->backing_asn1 = *src;
  memcpy(dst->intranet, dst->backing_asn1.intranet.address.choice.ipv4.buf,
         dst->backing_asn1.intranet.address.choice.ipv4.size);

  if (dst->backing_asn1.internet) {
    dst->backing_asn1inet = *dst->backing_asn1.internet;
    memcpy(dst->internet, dst->backing_asn1inet.address.choice.ipv4.buf,
           dst->backing_asn1inet.address.choice.ipv4.size);
  }
}

const PraefNetworkIdentifierPair_t*
praef_flat_netid_to_asn1(const praef_flat_netid* csrc) {
  praef_flat_netid* src = (praef_flat_netid*)csrc;

  /* Fix pointers up */
  src->backing_asn1.intranet.address.choice.ipv4.buf = src->intranet;
  if (src->backing_asn1.internet) {
    src->backing_asn1.internet = &src->backing_asn1inet;
    src->backing_asn1.internet->address.choice.ipv4.buf = src->internet;
  }

  return &src->backing_asn1;
}
