/*-
 * Copyright (c) 2013, 2014 Jason Lingle
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

#ifndef LIBPRAEFECTUS_BSD_H_
#define LIBPRAEFECTUS_BSD_H_

/* Since GNU's "libbsd" package for some reason puts the headers in a different
 * location, despite its sole purpose being to provide compatibility with
 * applications using the BSD extensions, we need this bit of logic to figure
 * out where the includes are.
 */

#if HAVE_BSD_ERR_H
#include <bsd/err.h>
#elif HAVE_ERR_H
#include <err.h>
#else
#error "No BSD err.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

#if HAVE_BSD_SYS_QUEUE_H
#include <bsd/sys/queue.h>
#elif HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#else
#error "No BSD sys/queue.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

#if HAVE_BSD_SYS_TREE_H
#include <bsd/sys/tree.h>
#elif HAVE_SYS_TREE_H
#include <sys/tree.h>
#else
#error "No BSD sys/tree.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

#if HAVE_BSD_SYSEXITS_H
#include <bsd/sysexits.h>
#elif HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#error "No BSD sysexits.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

#if HAVE_BSD_SYS_ENDIAN_H
#include <bsd/sys/endian.h>
#elif HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#else
#error "No BSD sys/endian.h could be found on your system. (See libbsd-dev on GNU.)"
#endif

/* In an older libbsd on Debian, we have the following comment in cdefs.h:
 *
 * * Linux headers define a struct with a member names __unused.
 * * Debian bugs: #522773 (linux), #522774 (libc).
 * * Disable for now.
 *
 * Following this is an #if 0 surrounding the definition of __unused, which
 * breaks RB_*. Why they couldn't just s/__unused/__bsd_unused/g to work around
 * this is beyond me. Since this is BSD-native code, we obviously won't be
 * including Linux-specific headers, so just replicate what's supposed to be
 * provided.
 */
#ifndef __unused
# if LIBBSD_GCC_VERSION >= 0x0300
#  define __unused __attribute__((unused))
# else
#  define __unused
# endif
#endif

/* Just in case we find a system that doesn't define these */
#ifndef __BEGIN_DECLS
#if defined(__cplusplus)
#define	__BEGIN_DECLS	extern "C" {
#define	__END_DECLS	}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif
#endif

/* DragonFly's queue.h doesn't define FOREACH_SAFE variants */
#ifndef SLIST_FOREACH_SAFE
#define SLIST_FOREACH_SAFE(var, head, name, temp_var)           \
    for ((var) = SLIST_FIRST(head);                             \
         (var) && ((temp_var) = SLIST_NEXT((var), name), 1);    \
         (var) = (temp_var))
#endif
#ifndef LIST_FOREACH_SAFE
#define LIST_FOREACH_SAFE(var, head, name, temp_var)           \
    for ((var) = LIST_FIRST(head);                             \
         (var) && ((temp_var) = LIST_NEXT((var), name), 1);    \
         (var) = (temp_var))
#endif
#ifndef STAILQ_FOREACH_SAFE
#define STAILQ_FOREACH_SAFE(var, head, name, temp_var)           \
    for ((var) = STAILQ_FIRST(head);                             \
         (var) && ((temp_var) = STAILQ_NEXT((var), name), 1);    \
         (var) = (temp_var))
#endif
#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, name, temp_var)           \
    for ((var) = TAILQ_FIRST(head);                             \
         (var) && ((temp_var) = TAILQ_NEXT((var), name), 1);    \
         (var) = (temp_var))
#endif

/* DragonFly's tree.h doesn't define RB_NFIND */
#ifndef RB_NFIND
/* XXX This only works on modern GCC/Clang.
 * Contained implementation adapted from FreeBSD's tree.h
 */
#define RB_NFIND_CMP(type, head, target, field, cmp)    \
    ({                                                  \
      typeof(target) _tmp = RB_ROOT(head);              \
      typeof(target) _res = NULL;                       \
      typeof(target) _target = (target);                \
      int _comp;                                        \
      while (_tmp) {                                    \
        _comp = cmp(_target, _tmp);                     \
        if (_comp < 0) {                                \
          _res = _tmp;                                  \
          _tmp = RB_LEFT(_tmp, field);                  \
        } else if (_comp > 0) {                         \
          _tmp = RB_RIGHT(_tmp, field);                 \
        } else {                                        \
          _res = _tmp;                                  \
          break;                                        \
        }                                               \
      }                                                 \
      _res;                                             \
    })
#else
#define RB_NFIND_CMP(type, head, target, field, cmp)    \
    RB_NFIND(type, head, target)
#endif

#endif /* LIBPRAEFECTUS_BSD_H_ */
