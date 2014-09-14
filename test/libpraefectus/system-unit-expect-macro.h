/* This file is supposed to be part of system-unit.c, but it seriously confuses
 * Emacs c-mode when present there for some reason.
 */

#define EXPECT(max, ...)                                        \
  do {                                                          \
    exchange _exchanges[] = {__VA_ARGS__};                      \
    do_expectation(_exchanges,                                  \
                   sizeof(_exchanges)/sizeof(_exchanges[0]),    \
                   max);                                        \
  } while (0)
