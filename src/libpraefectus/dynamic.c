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

#include "dynamic.h"

static praef_ivdr praef_ivd_id();
static praef_ivdr (*praef_do_ivd(void*, praef_ivdr,
                                 const praef_dynobj*,
                                 const char*))();

praef_ivdr (*praef_do_ivd_first(void* thisptr,
                                const praef_dynobj_chain* chain,
                                const char* method,
                                const void* dummy))() {
  return praef_do_ivd(thisptr, praef_ivdr_not_imp,
                      SLIST_FIRST(chain), method);
}

praef_ivdr (*praef_do_ivd_next(void* thisptr,
                               praef_ivdr def,
                               const void* vcurrent,
                               const char* method,
                               const void* dummy))() {
  praef_dynobj* current = (void*)vcurrent;
  return praef_do_ivd(thisptr, def, SLIST_NEXT(current, next), method);
}

static praef_ivdr (*praef_do_ivd(void* vthisptr,
                                 praef_ivdr def,
                                 const praef_dynobj* current,
                                 const char* method))() {
  void** thisptr = (void**)vthisptr;
  const praef_ivdm* ivdm;

  while (current) {
    for (ivdm = current->vtable; ivdm->name; ++ivdm) {
      if (0 == strcmp(method, ivdm->name)) {
        *thisptr = (void*)current;
        return ivdm->impl;
      }
    }

    current = SLIST_NEXT(current, next);
  }

  *thisptr = ((char*)NULL) + def;
  return praef_ivd_id;
}

static praef_ivdr praef_ivd_id(char* ptr) {
  return (praef_ivdr)(ptr - (char*)NULL);
}
