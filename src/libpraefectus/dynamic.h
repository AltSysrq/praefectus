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
#ifndef LIBPRAEFECTUS_DYNAMIC_H_
#define LIBPRAEFECTUS_DYNAMIC_H_

#include "common.h"

/**
 * @file
 *
 * This file defines an interface for performing dynamic (ie, untyped ad-hoc)
 * method executions on object chains.
 *
 * Rationale: We'd like the majority of the message handling to be done in
 * small, isolated components. However, some components will need to talk to
 * each other, possibly without knowing what other components are interesested
 * in the information. Giving components specific knowledge about others'
 * identities is undesirable, and defining an interface with one entry for
 * every method ever needed would be cumbersome and verbose.
 *
 * Instead, sacrifice type-safety (which we mostly give up in the interfaces
 * anyway via function pointer casts) for full decoupling.
 *
 * A dynamic object begins with a praef_dynobj entry, which identifies the
 * vtable for the object as well as the next object in the chain. A vtable is
 * simply a list of (name,function) pairs, where the function has typeless
 * arguments and returns a praef_ivdr response. An object is said to implement
 * a method if it contains an entry whose name corresponds with the method.
 *
 * To combat typos, one must first declare the existence of a method with the
 * PRAEF_IVD_DECL() macro. This does not pollute the global namespace and can
 * be used any number of times (including for the same method).
 *
 * Invoking a method on a chain is performed with the PRAEF_IVD macro, which
 * takes a pointer to the head of the chain, and the unqouted name of the
 * method. Additionally, a context argument must be given which will be
 * overwritten with sufficient information to execute the call. The result of
 * PRAEF_IVD is a non-null function (not function-pointer, technically, though
 * this is a mere technicallity in modern C) with typeless arguments. The
 * method is invoked by passing the context argument followed by the actual
 * arguments to the method. By convention, the context argument is named "_".
 *
 * Methods are conventionally unprefixed, but are written with reverse
 * Hungarian notation to indicate the types of their arguments other than the
 * this argument. Types are written beginning with a capital letter
 * corresponding to the first letter of the first word of the type, followed by
 * a letter for each following type. '*' is assumed to begin with 'p'. "_t" is
 * not considered a word. Eg: A method taking an unsigned, a const char*, and a
 * const PraefMsg_t*, would have a suffix of _UCcpCpmp.
 *
 * Invocation example:
 *
 *   / * Probably in a header somewhere * /
 *   PRAEF_IVD_DECL(do_something_I);
 *
 *   / * In the source * /
 *   void* _;
 *   praef_ivdr result;
 *
 *   result = PRAEF_IVD(&_, object_chain, do_something_I)(_, 42);
 *
 * The result of an invocation is praef_ivdr_not_imp if no object in the chain
 * implements the method. Otherwise, it is whatever the first implementing
 * object returned. (Note that typically a method will forward the call to
 * later objects in the chain.)
 *
 * An implementation is conventionally written as follows:
 *
 * 1. PRAEF_IVD_DECL()s in the header, if any.
 * 2. Definition of the object struct, beginning with a member of type
 *    praef_dynobj named self.
 * 3. Static function declarations for each of the methods. Each function is
 *    named <base>_<method>, where <base> is the name of the type. The
 *    declarations return praef_ivdr but have typeless arguments.
 * 4. A static constant of type praef_ivdm[], defining the vtable for the
 *    object. The contents of this array are specified with the PRAEF_IVDM()
 *    macro, and terminated with PRAEF_IVDM_END.
 * 5. Any conventional public APIs for the type. Often this is just a
 *    constructor.
 * 6. Implementations of the method functions. Each should take as its first
 *    argument a pointer to the object type, named "this".
 *
 * Except for predicate-like methods (eg, is_message_acceptable(_,msg)),
 * implementations should end with a PRAEF_IVD_NEXT(). This method is exactly
 * the same as PRAEF_IVD(), except that it takes a member of the chain (ie,
 * "this") and a default return value if no further objects implement the
 * method.
 *
 * Example implementation:
 *
 *   / * The header * /
 *   PRAEF_IVD_DECL(do_something_I);
 *
 *   typedef struct my_object_s my_object;
 *   my_object* my_object_new(void);
 *
 *   / * The source * /
 *   struct my_object_s {
 *     praef_dynobj self;
 *     int accum;
 *   };
 *
 *   static praef_ivdr my_object_do_something_I();
 *
 *   static praef_ivdm my_object_vtable[] = {
 *     PRAEF_IVDM(my_object, do_something_I),
 *     PRAEF_IVDM_END
 *   };
 *
 *   my_object* my_object_new(void) {
 *     my_object* this = malloc(sizeof(my_object));
 *     if (!this) return NULL;
 *     this->self.vtable = my_object_vtable;
 *     this->accum = 0;
 *     return this;
 *   }
 *
 *   static praef_ivdr my_object_do_something_I(my_object* this, int incr) {
 *     void* _;
 *     printf("Incrementing %d by %d (result=%d)\n",
 *            this->accum, incr, this->accum + incr);
 *     this->accum += incr;
 *     return PRAEF_IVD_NEXT(&_, this, praef_ivdr_imp, do_something_I)(_, incr);
 *   }
 *
 * tl;dr: It's the type-safety of Python with the beauty of C, plus
 * name-safety, so all in all not *that* bad.
 */

/**
 * Declares the method of the given name to exist. This is necessary in order
 * to use a method or include it in a vtable.
 *
 * This declaration can be repeated any number of times.
 *
 * Don't prefix methods with pseudo-namespaces. They exist in a namespace
 * unique to Praefectus dynamic methods; the very limited scope of usability
 * makes the chance of collision virtually non-existent.
 */
#define PRAEF_IVD_DECL(method) struct _praef_ivd_##method

/**
 * Invokes a dynamic method on an object chain.
 *
 * @param thisptr A pointer to a pointer of any type. The pointer will be
 * overwritten with the this of the implementing object, or possibly something
 * else pointer-sized.
 * @param chain A praef_dynobj_chain* in which implementing objects are to be
 * found.
 * @param method The unquoted name of the method to call. This must have been
 * declared with PRAEF_IVD_DECL().
 * @return A function with typeless arguments that returns praef_ivdr.
 */
#define PRAEF_IVD(thisptr, chain, method)                               \
  (*praef_do_ivd_first((thisptr), (chain), #method,                     \
                       (struct _praef_ivd_##method*)NULL))
/**
 * Invokes a method on the next object in the chain.
 *
 * @param thisptr A pointer to a pointer of any type. The pointer will be
 * overwritten with the this of the implementing object, or possibly something
 * else pointer-sized. If the containing statement is of the form
 *   return PRAEF_IVD_NEXT(...);
 * using `this` may be reasonable here.
 *
 * @param chain The this of the calling object (ie, can be cast to a
 * praef_dynobj*).
 * @param def The default return value if no further object in the chain
 * implements this method.
 * @param method The unquoted name of the method to call. This must have been
 * declared with PRAEF_IVD_DECL().
 * @return A function with typeless arguments that returns praef_ivdr.
 */
#define PRAEF_IVD_NEXT(thisptr, chain, def, method)                 \
  (*praef_do_ivd_next((thisptr), def, (chain), #method,             \
                      (struct _praef_ivd_##method*)NULL))

/**
 * Defines an entry in a dynamic method table.
 *
 * @param base The unquoted basename of the implementation.
 * @param method The unquoted name of the method being defined. The
 * implementation must be in a function named <base>_<method>. The method must
 * have been defined by PRAEF_IVD_DECL.
 */
#define PRAEF_IVDM(base, method)                \
  { (const char*)(const struct _praef_ivd_##method*)#method, base##_##method }

/**
 * Indicates the end of a dynamic method table. This must be the final entry of
 * any dynamic vtable.
 */
#define PRAEF_IVDM_END { NULL, NULL }

/**
 * Return type from dynamic invocations.
 */
typedef enum {
  /**
   * Indicates that no object in the chain supports the method. This is false
   * in the eyes of C.
   */
  praef_ivdr_not_imp = 0,
  /**
   * Indicates that at least one object in the chain implements the method, but
   * none of them had anything more specific to provide in their return value.
   */
  praef_ivdr_imp,
  /**
   * Indicates that at least one object in the chain implements the method, and
   * is giving an affirmative response. This is used for predicate-like
   * methods.
   */
  praef_ivdr_true,
  /**
   * Indicates that at least one object in the chain implements the method, and
   * is giving a negative response. This is used for predicate-like methods.
   *
   * NOTE: This is true in the eyes of C; test with
   *   if (praef_ivdr_false == x)
   * and not
   *   if (x)
   */
  praef_ivdr_false
} praef_ivdr;

/**
 * Defines a single entry of a dynamic method table.
 */
typedef struct {
  /**
   * The name of the method being described.
   */
  const char* name;
  /**
   * A pointer to the implementation of this method.
   */
  praef_ivdr (*impl)();
} praef_ivdm;

/**
 * The mandatory first element of any object with dynamic methods.
 */
typedef struct praef_dynobj_s {
  /**
   * The table of dynamic methods for this object.
   */
  const praef_ivdm* vtable;
  /**
   * The next object in the object chain.
   */
  SLIST_ENTRY(praef_dynobj_s) next;
} praef_dynobj;

/**
 * A linked-list of dynamic objects.
 */
typedef SLIST_HEAD(,praef_dynobj_s) praef_dynobj_chain;

/**
 * Internal implementation of PRAEF_IVD().
 */
praef_ivdr (*praef_do_ivd_first(void* thisptr,
                                const praef_dynobj_chain* chain,
                                const char* method,
                                const void* dummy))();

/**
 * Internal implementation of PRAEF_IVD_NEXT().
 */
praef_ivdr (*praef_do_ivd_next(void* thisptr,
                               praef_ivdr def,
                               const void* current,
                               const char* method,
                               const void* dummy))();

#endif /* LIBPRAEFECTUS_DYNAMIC_H_ */
