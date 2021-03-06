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
#ifndef LIBPRAEFECTUS_DEFS_H_
#define LIBPRAEFECTUS_DEFS_H_

/* Definitions for libpraefectus implementation.
 *
 * Do not include from a header file.
 */

#include <stddef.h>

/**
 * Given a type, the name of a member, and a pointer to such a member, return a
 * pointer to the base of the given type.
 */
#define UNDOT(type,member,ptr)                  \
  ((type*)(((char*)(ptr)) - offsetof(type, member)))

/* Since asn1c makes all its integer types "long", explicitly define our
 * intended sizes here.
 *
 * It would be nice to have something less fragile, though.
 */
#define SIZEOF_ASN1_SHORT 2
#define SIZEOF_ASN1_DWORD 4

#else /* LIBPRAEFECTUS_DEFS_H_ already defined... */

#error "defs.h" included twice. This probably means a header included it, which is forbidden.

#endif /* LIBPRAEFECTUS_DEFS_H_ */
