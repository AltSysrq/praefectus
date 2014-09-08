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

#include <stdlib.h>

#include "font.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#if 0
/* template */
  { "NAME",
    ""
    ""
    ""
    ""
    ""
    ""
    ""
    ""
    ""
    ""
    "" /*^^baseline*/
    ""
    "" /*^^em*/
    "" },
#endif /* 0 */

/* Much of this was based off of Angelican Text (at least as far as getting a
 * rough idea of what the character should look like).
 */

static font_char fraktur_chars[] = {
  { " ",
    "   "
    "   "
    "   "
    "   "
    "   "
    "   "
    "   "
    "   "
    "   "
    "   "
    "   " /*^^baseline*/
    "   "
    "   " /*^^em*/
    "   ",
    { 0, 0, 0 },
    { 3, 3, 3 },
    1 },
  { "A",
    "    #     "
    "    #     "
    "   # #    "
    "   # ##   "
    "   # ##   "
    " ###  ##  "
    "# # ####  "
    "  # #### #"
    " #   # ## "
    "##   # ## "
    "#     =## " /*^^baseline*/
    "          "
    "          " /*^^em*/
    "          " },
  { "B",
    " ## ##   "
    "#-## ### "
    "  ##  ## "
    "  ##  #  "
    "  ## #   "
    " ###= #  "
    "# ##  ## "
    "  ##  ## "
    " #### #  "
    "# #  #   "
    "         " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "C",
    "   #-##  "
    "  #-#  # "
    " #-## ---"
    " #-##    "
    "#- ##    "
    "#- ##    "
    " #-##  = "
    " #- #  # "
    "  #- # # "
    "   #- #  "
    "    ##   " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "D",
    "=       "
    " #####  "
    "  ##### "
    "  #  ## "
    " ##  ## "
    " ##  ## "
    "=##  ## "
    " ##  ## "
    " #-  ## "
    "= ## #  "
    " =###   " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        " },
  { "E",
    " =  #   "
    "  ##-## "
    " ###  # "
    "#-##    "
    "#-##  # "
    "#-####- "
    "#-##  # "
    "#-##    "
    "#-#   # "
    " #   #= "
    "  ###   " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        " },
  { "F",
    " =   ##  "
    "  ###-## "
    " #-##  # "
    " #-##    "
    " #-##  # "
    "=#-####- "
    " #-##  # "
    " #-##    "
    " #-##    "
    "=- ##    "
    " ==  =   " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "G",
    "   #-##   "
    "  #-#  #  "
    " #-## --- "
    " #-##     "
    "#- ##     "
    "#- ## =   "
    " #-##  ## "
    " #- #  ## "
    "  #- # ## "
    "   #- # = "
    "    ##    " /*^^baseline*/
    "          "
    "          " /*^^em*/
    "          " },
  { "H",
    "=   =   "
    " ##  ## "
    " ##  ## "
    " ##   # "
    " ## ##  "
    " ###### "
    " ### -# "
    " ##  ## "
    " ##  ## "
    " ##  ## "
    " #   #  " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        " },
  { "I",
    " #  # "
    "# ## #"
    "  ##  "
    "  ##  "
    " ###  "
    "# ##  "
    "  ##  "
    "  ##  "
    "  ##  "
    "# ## #"
    " #  # " /*^^baseline*/
    "      "
    "      " /*^^em*/
    "      " },
  { "J",
    " ##  # "
    "#  ## #"
    "   ##  "
    "   ##  "
    "   ##  "
    "   ##  "
    "## ##  "
    "## ##  "
    "#  ##  "
    "## ## #"
    " ##  # " /*^^baseline*/
    "       "
    "       " /*^^em*/
    "       " },
  { "K",
    "#  ##   "
    " ##  #  "
    " ##     "
    " ##  #  "
    " ## ### "
    " ### ## "
    " ##   # "
    " ####   "
    " #   #  "
    " ##  ## "
    "#  # ## " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        " },
  { "L",
    "  ##    "
    " ## #   "
    "# #     "
    " ##     "
    " ##     "
    " ##     "
    "=##     "
    " ##   # "
    " ##   -#"
    " ###  # "
    "#  ###  " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        " },
  { "M",
    "   ##  ##   "
    "  #  ##  ## "
    " ##  ##  ## "
    " ##  ##  ## "
    " ##  ##  ## "
    " ##  ##  ## "
    "=## #### ## "
    " ##  ##  ## "
    " ##  ##  ## "
    " ##  ##  ## "
    "# ##  #    #" /*^^baseline*/
    "            "
    "            " /*^^em*/
    "            " },
  { "N",
    "   ##  # "
    "  #  ## -"
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    "=##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    "##  # ## " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "O",
    "#   #   "
    " # ##   "
    "  ## #  "
    "  ## ## "
    " ## ### "
    " ##  ## "
    " ##  ## "
    " ##  ## "
    "  #  #  "
    "  # ##  "
    "   ##   " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        " },
  { "P",
    "#  ##   "
    " ##  #  "
    " ##  ## "
    " ##  ## "
    " ##  ## "
    "#####   "
    " ##     "
    " ##     "
    " #      "
    " ##     "
    "#  ##   " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        ",
    { 0, 0, 0 },
    { 7, 7, 5 },
  },
  { "Q",
    "#   #    "
    " # ##    "
    "  ## #   "
    "  ## ##  "
    " ## ###  "
    " ##  ##  "
    " ##  ##  "
    " ### ##  "
    "  # ##   "
    "  # ###  "
    "   ## ## " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "R",
    "#  ##   "
    " ##  #  "
    " ##  ## "
    " ##  ## "
    "######  "
    " ## #   "
    " ##  #  "
    " #   ## "
    " ##  ## "
    "#  #  # "
    "        " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        ",
    { 0, 0, 0 },
    { 7, 7, 7 },
  },
  { "S",
    "  ##  "
    " #  # "
    "#  ## "
    "##    "
    "###   "
    " ###  "
    "  ### "
    "   ## "
    "## ## "
    "#  #  "
    " ##   " /*^^baseline*/
    "      "
    "      " /*^^em*/
    "      " },
  { "T",
    " ########"
    "######## "
    "    #    "
    " #-##    "
    " #-##    "
    "#- ##    "
    "#- ##    "
    "#- ## #  "
    " #-##  # "
    " #-# # # "
    "  #   #  " /*^^baseline*/
    "   ###   "
    "         " /*^^em*/
    "         " },
  { "U",
    "#  #  #  #"
    " ##    ## "
    " ##    ## "
    " ##    ## "
    " ##    ## "
    "###    ## "
    " ##    ## "
    " ##    ## "
    " ##    ## "
    "  ##  ##  "
    "    ##    " /*^^baseline*/
    "          "
    "          " /*^^em*/
    "          " },
  { "V",
    " ##     = #"
    "= ##     # "
    "  ##     # "
    "  ##     # "
    "   ##   #  "
    "  ###   #  "
    " # ##   #  "
    "##  ## #   "
    "    ## #   "
    "    ## #   "
    "      #    " /*^^baseline*/
    "           "
    "           " /*^^em*/
    "           " },
  { "W",
    "#  #     # "
    " ##     # #"
    " ##  # ##  "
    " ## ## ##  "
    " ## ## ##  "
    "### ## ##  "
    " ## ## ##  "
    " ## ## ##  "
    " #  #  ##  "
    " # ##  #   "
    "# #####    " /*^^baseline*/
    "           "
    "           " /*^^em*/
    "           " },
  { "X",
    " #      # "
    "# #     # "
    "  ##   #  "
    "  ### #   "
    "   ## #   "
    "    ##    "
    "    ###   "
    "   # ##   "
    "   #  ##  "
    "# #   ##  "
    " #     ## " /*^^baseline*/
    "          "
    "          " /*^^em*/
    "          " },
  { "Y",
    " #     # "
    "##     # "
    " ##   #  "
    " ##   #  "
    "  ## #   "
    "  ## #   "
    "   ##    "
    "  ###    "
    " # ##    "
    "   ##    "
    "   #     " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "Z",
    " ######  "
    "##    ## "
    "      ## "
    "     ##  "
    " ## ##   "
    "#  ##  # "
    "  ## ##  "
    " ##      "
    "##       "
    "##    ## "
    " ######  " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "a",
    "         "
    "         "
    "  ####   "
    " -   ##  "
    " ### ##  "
    "##  ###  "
    "##   ##  "
    "##   ##  "
    "### ###  "
    " ### ### "
    "  #   #  " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "b",
    "   =    "
    "=##     "
    " ##     "
    " ##  #  "
    " ## ### "
    " ### ## "
    " ##  ## "
    " ##  ## "
    " ### #  "
    "  ###   "
    "   #    " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        " },
  { "c",
    "       "
    "       "
    "       "
    "   ##  "
    "  # ## "
    " #   # "
    "##     "
    "##     "
    "###    "
    " ##  # "
    "  ###  " /*^^baseline*/
    "       "
    "       " /*^^em*/
    "       " },
  { "d",
    "  ##   "
    "  ##   "
    "   ##  "
    "  # ## "
    " #  ## "
    "##  ## "
    "##  ## "
    "##  ## "
    " ## ## "
    " ## #  "
    "  ##   " /*^^baseline*/
    "       "
    "       " /*^^em*/
    "       " },
  { "e",
    "       "
    "       "
    "       "
    "   ##  "
    "  # ## "
    " #   # "
    "## ##  "
    "###    "
    "###    "
    " ##  # "
    "  ###  " /*^^baseline*/
    "       "
    "       " /*^^em*/
    "       " },
  { "f",
    "  ###  "
    " # # = "
    "##     "
    "##     "
    "####   "
    "### =  "
    "##     "
    "##     "
    "##     "
    "##     "
    "#  -   " /*^^baseline*/
    " ==    "
    "       " /*^^em*/
    "       " },
  { "g",
    "       "
    "       "
    "       "
    "   #   "
    "  # ## "
    " #  ## "
    "##  ## "
    "##  ## "
    "##  ## "
    " ### # "
    "    ## " /*^^baseline*/
    " #  #  "
    " # #   " /*^^em*/
    "  #    " },
  { "h",
    "  #    "
    " #     "
    "##     "
    "##  #  "
    "## ##  "
    "### #  "
    "## ##  "
    "## ##  "
    "## ##  "
    "## ##  "
    " #  ## " /*^^baseline*/
    "       "
    "       " /*^^em*/
    "       " },
  { "i",
    " =  "
    "=#= "
    " =  "
    "    "
    " #  "
    "### "
    " ## "
    " ## "
    " ## "
    " ## "
    "  # " /*^^baseline*/
    "    "
    "    " /*^^em*/
    "    " },
  { "j",
    "   =  "
    "  =#= "
    "   =  "
    "      "
    "   #  "
    "  ### "
    "   ## "
    "   ## "
    "   ## "
    "   ## "
    "   ## " /*^^baseline*/
    "#   # "
    "## #  " /*^^em*/
    " ##   " },
  { "k",
    "  #     "
    " #      "
    "##      "
    "## ##   "
    "### ##  "
    "##  ##  "
    "## #    "
    "####    "
    "##  ##  "
    "##  ##  "
    " #   #= " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        " },
  { "l",
    "   = "
    "# #  "
    " ##  "
    " ##  "
    " ##  "
    " ##  "
    " ##  "
    " ##  "
    " ##  "
    " ##  "
    " #   " /*^^baseline*/
    "     "
    "     " /*^^em*/
    "     " },
  { "m",
    "           "
    "           "
    "           "
    " # ## ##   "
    "### ## ##  "
    " ## ## ##  "
    " ## ## ##  "
    " ## ## ##  "
    " ## ## ##  "
    " ## ## ##  "
    " #  #   ## " /*^^baseline*/
    "           "
    "           " /*^^em*/
    "           " },
  { "n",
    "         "
    "         "
    "         "
    " #  ##   "
    "#### ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " #    ## " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "o",
    "        "
    "        "
    "   ##   "
    "  # ##  "
    " #   ## "
    "##   ## "
    "##   ## "
    "##   ## "
    " ##  #  "
    " ## #   "
    "  ##    " /*^^baseline*/
    "        "
    "        " /*^^em*/
    "        " },
  { "p",
    "        "
    "        "
    "        "
    "#   #   "
    " ### #  "
    " ##  ## "
    " ##  ## "
    " ##  ## "
    " ##  ## "
    " ### #  "
    " ## #   " /*^^baseline*/
    " ##     "
    " # =    " /*^^em*/
    "#       " },
  { "q",
    "         "
    "         "
    "         "
    "    #    "
    "  ## #   "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    "  # ###  "
    "   # ##  " /*^^baseline*/
    "     ##  "
    "     # # " /*^^em*/
    "    =    " },
  { "r",
    "       "
    "       "
    "       "
    " # #   "
    "#####  "
    " ## #= "
    " ## =  "
    " ##    "
    " ##    "
    " ##    "
    " #     " /*^^baseline*/
    "       "
    "       " /*^^em*/
    "       " },
  { "s",
    "       "
    "       "
    "  ##   "
    " # ##  "
    "##  ## "
    "##  -  "
    "  ##   "
    " -  ## "
    "##  ## "
    " ## #  "
    "  ##   " /*^^baseline*/
    "       "
    "       " /*^^em*/
    "       " },
  { "t",
    "      "
    "# #   "
    " ##   "
    " ##   "
    "####  "
    " ## # "
    " ##   "
    " ##   "
    " ##   "
    " ##   "
    "  ##  " /*^^baseline*/
    "      "
    "      " /*^^em*/
    "      " },
  { "u",
    "         "
    "         "
    "         "
    " #   #   "
    "### ###  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    "   ##  # " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "v",
    "         "
    "         "
    "#        "
    "##   #   "
    " ## ###  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  #   "
    "   ##    " /*^^baseline*/
    "         "
    "         " /*^^em*/
    "         " },
  { "w",
    "            "
    "            "
    "#           "
    "##   #   #  "
    " ## ### ### "
    " ##  ##  ## "
    " ##  ##  ## "
    " ##  ## ##  "
    " ##  ##  ## "
    " ##  ### ## "
    "   ##  ##   " /*^^baseline*/
    "            "
    "            " /*^^em*/
    "            " },
  { "x",
    "       "
    "       "
    "       "
    " ## =  "
    "###  # "
    "  ## # "
    "  ###  "
    " ###   "
    "# ##   "
    "# ## # "
    "=  ### " /*^^baseline*/
    "       "
    "       " /*^^em*/
    "       " },
  { "y",
    "         "
    "         "
    "         "
    " #   #   "
    "### ###  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    " ##  ##  "
    "   ##    " /*^^baseline*/
    "     ##  "
    "  #  ##  " /*^^em*/
    "   ###   " },
  { "z",
    "      "
    "      "
    "      "
    " ##   "
    "# ##  "
    "   ## "
    "  #   "
    " #    "
    "###   "
    " ###  "
    "   #  " /*^^baseline*/
    "  #   "
    "  #   " /*^^em*/
    " #    " },
 { NULL }
};

const font_spec fraktur = {
  14, 10, 12, fraktur_chars
};
