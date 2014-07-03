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

#include "std-state.h"

int praef_std_state_init(praef_std_state* this) {
  memset(this, 0, sizeof(*this));
  if (!(this->context = praef_context_new())) goto fail;
  if (!(this->tx = praef_transactor_new(this->context))) goto fail;
  praef_mtt_bridge_init(&this->bridge, this->tx);
  if (!(this->mtx = praef_metatransactor_new(
          (praef_metatransactor_cxn*)&this->bridge))) goto fail;

  return 1;

  fail:
  if (this->mtx) free(this->mtx);
  if (this->tx) free(this->tx);
  if (this->context) free(this->context);
  return 0;
}

void praef_std_state_cleanup(praef_std_state* this) {
  free(this->mtx);
  free(this->tx);
  free(this->context);
}

void praef_std_state_advance(praef_std_state* this,
                             unsigned delta,
                             praef_userdata userdata) {
  praef_metatransactor_advance(this->mtx, delta);
  praef_context_advance(praef_transactor_master(this->tx), delta, NULL);
  praef_context_advance(this->context, delta, userdata);
}
