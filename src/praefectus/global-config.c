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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "bsd.h"

#include "global-config.h"

PraefectusConfiguration_t global_config;
char global_config_screen_name[17];

static int config_gen_filename(char* dst, size_t sz) {
  const char* appdata, * home;

  appdata = getenv("APPDATA");
  home = getenv("HOME");
  if (appdata) {
    /* Windows convention */
    return (size_t)snprintf(dst, sz, "%s\\Praefectus.xml", appdata) < sz;
  } else if (home) {
    /* UNIX convention */
    return (size_t)snprintf(dst, sz, "%s/.praefectus.xml", home) < sz;
  } else {
    return 0;
  }
}

static const char*const kana[] = {
  "a", "ka", "ga", "sa", "za", "ta", "da", "na", "ha", "pa", "ba",
  "ma", "ya", "ra", "wa",
  "i", "ki", "gi", "shi", "sha", "shu", "sho", "ji", "ja", "ju", "jo",
  "chi", "cha", "chu", "cho", "ni", "hi", "pi", "bi", "mi", "ri",
  "u", "ku", "gu", "su", "zu", "tsu", "dzu", "nu", "fu", "pu", "bu",
  "mu", "yu", "ru",
  "e", "ke", "ge", "se", "ze", "te", "de", "ne", "he", "pe", "be",
  "me", "re",
  "o", "ko", "go", "so", "zo", "to", "do", "no", "ho", "po", "bo",
  "mo", "yo", "ro",
};

static void generate_default_screen_name(void) {
  time_t now = time(0);

  unsigned a, b, c, t, n = sizeof(kana)/sizeof(kana[0]);

  t = now % 100;
  now /= n;
  c = now % n;
  now /= n;
  b = now % n;
  now /= n;
  a = now % n;

  snprintf(global_config_screen_name, sizeof(global_config_screen_name),
           "%s%s%s%02d", kana[a], kana[b], kana[c], t);
}

void load_config(void) {
  char filename[256];
  FILE* in;
  char data[4096];
  size_t nread, data_size;
  unsigned off = 0;
  asn_codec_ctx_t ctx;
  asn_dec_rval_t result;
  PraefectusConfiguration_t* global_config_ptr = &global_config;

  if (!config_gen_filename(filename, sizeof(filename))) {
    warnx("Unable to determine location of configuration, using defaults.");
    goto use_defaults;
  }

  in = fopen(filename, "r");
  if (!in) {
    warn("Failed to open configuration in %s, using defaults", filename);
    goto use_defaults;
  }

  memset(&global_config, 0, sizeof(global_config));
  memset(&ctx, 0, sizeof(ctx));
  do {
    nread = fread(data+off, 1, sizeof(data)-off, in);
    if (0 == nread) {
      if (feof(in))
        warnx("Unexpected EOF reading %s, using defaults", filename);
      else
        warn("I/O error reading %s, using defaults", filename);

      fclose(in);
      goto use_defaults;
    }

    data_size = nread + off;
    result = xer_decode(&ctx, &asn_DEF_PraefectusConfiguration,
                        (void**)&global_config_ptr,
                        data, data_size);

    if (RC_WMORE == result.code) {
      off = data_size - result.consumed;
      memmove(data, data + result.consumed, off);

      if (off >= sizeof(data)) {
        warnx("Out of buffer space reading %s, using defalts", filename);
        fclose(in);
        goto use_defaults;
      }
    }
  } while (RC_WMORE == result.code);

  fclose(in);

  if (RC_OK != result.code) {
    warnx("Error in configuration in %s, using defaults", filename);
    goto use_defaults;
  }

  if ((*asn_DEF_PraefectusConfiguration.check_constraints)(
        &asn_DEF_PraefectusConfiguration, &global_config, NULL, NULL)) {
    warnx("Configuration in %s is invalid, using defaults", filename);
    goto use_defaults;
  }

  strlcpy(global_config_screen_name, (char*)global_config.screenname.buf,
          sizeof(global_config));
  free(global_config.screenname.buf);
  global_config.screenname.buf = (void*)global_config_screen_name;

  /* OK */
  return;

  use_defaults:
  (*asn_DEF_PraefectusConfiguration.free_struct)(
    &asn_DEF_PraefectusConfiguration, &global_config, 1);
  memset(&global_config, 0, sizeof(global_config));
  global_config.bel = PraefectusConfiguration__bel_flash;
  generate_default_screen_name();
  global_config.screenname.buf = (void*)global_config_screen_name;
  global_config.screenname.size = strlen((char*)global_config.screenname.buf);
  global_config.controls.up = SDLK_w;
  global_config.controls.left = SDLK_a;
  global_config.controls.down = SDLK_s;
  global_config.controls.right = SDLK_d;
  global_config.controls.talk = SDLK_t;
}

void save_config(void) {
  char filename[256];
  FILE* out;

  global_config.screenname.size = strlen((char*)global_config.screenname.buf);
  if (!config_gen_filename(filename, sizeof(filename))) {
    warnx("Unable to determine where to save configuration");
    return;
  }

  out = fopen(filename, "w");
  if (!out) {
    warn("Unable to open %s to save configuration", filename);
    return;
  }

  if (xer_fprint(out, &asn_DEF_PraefectusConfiguration, &global_config))
    warnx("Failed to write configuration to %s", filename);

  fclose(out);
}
