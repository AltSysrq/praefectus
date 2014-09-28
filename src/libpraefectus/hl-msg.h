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
#ifndef LIBPRAEFECTUS_HL_MSG_H_
#define LIBPRAEFECTUS_HL_MSG_H_

#include "messages/PraefMsg.h"
#include "common.h"
#include "dsa.h"

/**
 * The minumum possible MTU for messages produced by the hlmsg encoder.
 */
#define PRAEF_HLMSG_MTU_MIN                     \
  /* pubkey hint */                             \
  (2 +                                          \
   /* Signature */                              \
   PRAEF_SIGNATURE_SIZE +                       \
   /* Flags */                                  \
   1 +                                          \
   /* Instant */                                \
   4 +                                          \
   /* Serno */                                  \
   4 +                                          \
   /* Size, largest possible message */         \
   1 + 255)

#define PRAEF_HLMSG_JOINACCEPT_MAX 240

/**
 * The type for advisory serial numbers attached to hlmsgs.
 */
typedef unsigned praef_advisory_serial_number;

/**
 * A hlmsg is simply a data pointer paired with a size. It represents a single
 * high-level message (see DESIGN.txt for particulars of what a high-level
 * message is).
 *
 * For simplicity of implementation, an extra trailing 0 byte is required to be
 * appended to the hlmsg in-memory, though this is not sent over the wire.
 */
typedef struct {
  /**
   * The number of bytes behind data. This includes the trailing 0 byte that is
   * not part of the message proper.
   */
  size_t size;
  /**
   * The data for this hlmsg. This is generally owned by whoever owns the
   * hlmsg.
   *
   * Note that data[size-1] MUST be zero, even though this byte is not sent
   * over the wire.
   */
  const void* data;
} praef_hlmsg;

/**
 * Opaque type used to represent a pointer into a segment within an hlmsg.
 *
 * (This is actually a pointer into the data array of an hlmsg, so is therefore
 * dependent on the lifecycle of the hlmsg's data array.)
 */
typedef struct praef_hlmsg_segment_s praef_hlmsg_segment;

/**
 * The possible types derived from the lowest two flag bits.
 */
typedef enum {
  /**
   * Indicates that the contents of the hlmsg can be redistributed immediately,
   * and that they do not contribute to commit hashing.
   */
  praef_htf_uncommitted_redistributable,
  /**
   * Indicates that the contents of the hlmsg can be redistributed (but
   * preferably based upon other nodes' commit thresholds), and that they do
   * contribute to commit hashing.
   */
  praef_htf_committed_redistributable,
  /**
   * Indicates that the contents of the hlmsg are RPC-type messages.
   */
  praef_htf_rpc_type
} praef_hlmsg_type_flag;

/**
 * Returns whether the given hlmsg is structurally valid. This MUST be checked
 * before using any other hlmsg-decoding function.
 *
 * This checks both that the high-level message itself is sensible, and that
 * all the messages it contains can be decoded and are of the correct type (wrt
 * the high-level messages type), as well as that any constraints (both
 * internal and applying to the hlmsg itself) imposed by the contained messages
 * are met.
 *
 * If the constraint that the final byte of data is zero is violated, this
 * aborts the process instead, since it is indicative of a serious internal bug
 * rather than a failure of the message transmitter.
 */
int praef_hlmsg_is_valid(const praef_hlmsg*);

/**
 * Returns the public key hint of the given hlmsg.
 */
praef_pubkey_hint praef_hlmsg_pubkey_hint(const praef_hlmsg*);
/**
 * Returns a pointer to the raw signature within the hlmsg's data.
 */
unsigned char* praef_hlmsg_signature(const praef_hlmsg*);
/**
 * Returns the type of the hlmsg as defined by its flags.
 */
praef_hlmsg_type_flag praef_hlmsg_type(const praef_hlmsg*);
/**
 * Returns the instant of the given hlmsg.
 */
praef_instant praef_hlmsg_instant(const praef_hlmsg*);
/**
 * Returns the advisory serial number of the given hlmsg.
 */
praef_advisory_serial_number praef_hlmsg_serno(const praef_hlmsg*);

/**
 * Returns a pointer to the beginning of the data within the hlmsg which are
 * (supposedly) signed by the signature.
 */
const void* praef_hlmsg_signable(const praef_hlmsg*);
/**
 * Returns the number of bytes in the hlmsg which are (supposedly) signed by
 * the signature.
 */
size_t praef_hlmsg_signable_sz(const praef_hlmsg*);

/**
 * Returns a pointer to the first segment of the given hlmsg. This is
 * guaranteed to be non-NULL.
 */
const praef_hlmsg_segment* praef_hlmsg_first(const praef_hlmsg*);
/**
 * Returns a pointer to the segment following the given segment, or NULL if
 * this is the last segment in the hlmsg.
 */
const praef_hlmsg_segment* praef_hlmsg_snext(const praef_hlmsg_segment*);
/**
 * Decodes the data behind the given hlmsg segment. This is guaranteed to be
 * non-NULL (unless allocation fails) and is a message type appropriate for
 * this hlmsg.
 */
PraefMsg_t* praef_hlmsg_sdec(const praef_hlmsg_segment*);

/**
 * Creates a new hlmsg containing exactly the given data. The incoming data is
 * assumed to not already have the trailing zero byte; this is appended
 * implicitly.
 *
 * The memory can be released by passing the hlmsg to free(). This also frees
 * the data itself.
 *
 * @param data The data for the hlmsg. This is copied, with a zero byte
 * implicitly appended.
 * @param sz The number of bytes to copy from data. The resulting hlmsg will
 * have a size of (sz+1) due to the extra trailing zero byte.
 * @return The new hlmsg, or NULL if allocation fails.
 */
praef_hlmsg* praef_hlmsg_of(const void* data, size_t sz);


/**
 * An hlmsg_encoder encodes and aggregates PraefMsg objects into signed
 * high-level messages.
 *
 * Note that the encoder aborts the program if it is requested to encode
 * something impossible. This includes nested messages longer than 255 bytes,
 * or so large that they overflow the MTU in a single hlmsg, as well as
 * messages of a type inappropriate for the hlmsg.
 */
typedef struct praef_hlmsg_encoder_s praef_hlmsg_encoder;

/**
 * Creates a new, empty hlmsg_encoder.
 *
 * @param type The type of the messages to be generated.
 * @param signator The signator to use to sign the messages produced. This MAY
 * be NULL, in which cas the contents of the pubkey hint and signature fields
 * is undefined.
 * @param sn A pointer to an advisory serial number. If non-NULL, generated
 * messages will have advisory serial numbers generated by postincrementing the
 * value to which this argument points. If NULL, a value private to this
 * encoder will be used.
 * @param mtu The maximum permissible hlmsg to produce (not including the
 * trailing zero byte). This MUST be at least
 * (PRAEF_HLMSG_MTU_MIN+append_garbage).
 * @param append_garbage The number of bytes to reserve at the end of each
 * high-level message for garbage. This is important for committed messages to
 * ensure they are unpredictable. The trailing bytes are filled with
 * securely-generated random numbers, except that the first is always
 * zero. This value may not be 1.
 * @return The new encoder, or NULL if there is insufficient memory or the
 * parameters are invalid.
 */
praef_hlmsg_encoder* praef_hlmsg_encoder_new(praef_hlmsg_type_flag type,
                                             praef_signator* signator,
                                             praef_advisory_serial_number* sn,
                                             size_t mtu,
                                             size_t append_garbage);
/**
 * Frees the memory held by the given hlmsg_encoder.
 */
void praef_hlmsg_encoder_delete(praef_hlmsg_encoder*);

/**
 * Encodes and appends a new message through the encoder. If the current
 * accumulator has insufficient space for the new message, the old accumulator
 * is copied into *dst and a new high-level message is initiated.
 *
 * This call will put the encoder into a non-empty state.
 *
 * @param dst An output message whose size is at least (mtu+1). If a message is
 * generated, the data are overwritten and the size changed to the actual size
 * of the message. It is the caller's responsibility to later reset the size if
 * they wish to reuse the message object for further append calls.
 * @return Whether a new high-level message was produced (and encoded into
 * *dst).
 */
int praef_hlmsg_encoder_append(praef_hlmsg* dst,
                               praef_hlmsg_encoder*,
                               const PraefMsg_t*);
/**
 * Returns the timestamp that will be attached to new hlmsgs produced by the
 * given encoder.
 */
praef_instant praef_hlmsg_encoder_get_now(const praef_hlmsg_encoder*);

/**
 * Changes the timestamp attached to further hlmsgs produced by the given
 * encoder. The encoder MUST currently be empty.
 */
void praef_hlmsg_encoder_set_now(praef_hlmsg_encoder*, praef_instant);

/**
 * Encodes a high-level message containing exactly one message into the given
 * hlmsg. This always overwrites *dst, unlike append().
 *
 * This call does not alter the encoder's emptiness state.
 *
 * @param dst An output message whose size is at least (mtu+1). The data are
 * overwritten and the size changed to the actual size of the message. It is
 * the caller's responsibility to later reset the size if they wish to reuse
 * the message object for further append calls.
 */
void praef_hlmsg_encoder_singleton(praef_hlmsg* dst,
                                   praef_hlmsg_encoder*,
                                   const PraefMsg_t*);
/**
 * Ensures that there is no pending accumulating data in the given encoder. If
 * there is, the remaining high-level message is copied into *dst.
 *
 * After this call, the encoder is guaranteed to be empty.
 *
 * @param dst An output message whose size is at least (mtu+1). If a message is
 * generated, the data are overwritten and the size changed to the actual size
 * of the message. It is the caller's responsibility to later reset the size if
 * they wish to reuse the message object for further append calls.
 * @return Whether a new high-level message was produced (and encoded into
 * *dst).
 */
int praef_hlmsg_encoder_flush(praef_hlmsg* dst, praef_hlmsg_encoder*);

/**
 * Dumps the given message to stderr in human-readable format.
 *
 * This is intended only for debugging.
 */
void praef_hlmsg_debug_dump(const praef_hlmsg*);
/**
 * Like praef_hlmsg_debug_dump(), but takes a raw data array and size pair.
 */
void praef_hlmsg_debug_ddump(const void*, size_t);

#endif /* LIBPRAEFECTUS_HL_MSG_H_ */
