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
#ifndef LIBPRAEFECTUS_MTT_BRIDGE_H_
#define LIBPRAEFECTUS_MTT_BRIDGE_H_

#include "transactor.h"
#include "metatransactor.h"

/**
 * The praef_mtt_bridge provides a bridge from a praef_metatransactor_cxn to
 * the equivalent primitives on a praef_transactor.
 *
 * Instances of this struct should be initialised with
 * praef_mtt_bridge_init().
 */
typedef struct {
  /**
   * The cxn to be given to the metatransactor. (Usually, one casts fhe
   * praef_mtt_bridge* itself to a praef_metatransactor_cxn* for clarity,
   * though.)
   */
  praef_metatransactor_cxn cxn;
  /**
   * The transactor to which this bridge delegates.
   */
  praef_transactor* tx;
} praef_mtt_bridge;

/**
 * Fully initialises the given praef_mtt_bridge.
 *
 * @param dst The bridge to initialise. The entire structure will be
 * overwritten.
 * @param tx The transactor to use for this bridge.
 */
void praef_mtt_bridge_init(praef_mtt_bridge* dst, praef_transactor* tx);

#endif /* LIBPRAEFECTUS_MTT_BRIDGE_H_ */
