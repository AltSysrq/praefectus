/*-
 * Copyright (c) 2016, Jason Lingle
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
#ifndef LIBPRAEFECTUS_SIMPLE_H_
#define LIBPRAEFECTUS_SIMPLE_H_

#include "common.h"
#include "system.h"
#include "dsa.h"

/**
 * @file
 *
 * Simplification of the full praefectus API, for the general case of simply
 * wanting everything wired together in the normal way. This also simplifies
 * memory ownership semantics and eliminates the direct use of ASN.1 types,
 * mainly to make it easier to bind other languages to praefectus.
 *
 * Ownership and lifetime semantics are denoted using something similar to
 * Rust's notation.
 */

/**
 * Opaque struct representing a fully-assembled simplified system.
 */
typedef struct praef_simple_context_s praef_simple_context;

/**
 * Flattened representation of PraefIpAddress_t.
 *
 * This is nominally a union, but is laid out as a normal structure to simplify
 * usage via an FFI.
 */
typedef struct praef_simple_ip_address_s {
  /**
   * The IP version. Either 4 or 6.
   */
  unsigned char version;
  /**
   * If version==4, the 4 octets defining the IPv4 address.
   */
  unsigned char v4[4];
  /**
   * If version==6, the 8 integers (in native byte order) defining the IPv6
   * address.
   */
  unsigned short v6[8];
} praef_simple_ip_address;

/**
 * Flattened representation of PraefNetworkIdentifier_t.
 */
typedef struct praef_simple_netid_s {
  /**
   * The IP address.
   */
  praef_simple_ip_address address;
  /**
   * The port number, in native byte order.
   */
  unsigned short port;
} praef_simple_netid;

/**
 * Flattened representation of PraefNetworkIdentifierPair_t.
 */
typedef struct praef_simple_netid_pair_s {
  /**
   * If non-zero, the internet field is meaningful and this is a global
   * identifier pair. Otherwise, internet is unset and this is a local
   * identifier pair.
   */
  unsigned char global;
  /**
   * The local network address/port.
   */
  praef_simple_netid intranet;
  /**
   * The network address/port as seen by the internet at large. This is only
   * set if global is true.
   */
  praef_simple_netid internet;
} praef_simple_netid_pair;

/**
 * Flattened representation of PraefMsgJoinRequest_t.
 */
typedef struct praef_simple_join_request_s {
  /**
   * The public key field, encoded as defined in messages.asn1.
   */
  unsigned char public_key[PRAEF_PUBKEY_SIZE];
  /**
   * The identifier of the source host.
   */
  praef_simple_netid_pair identifier;
  /**
   * The actual length of the auth array. A value of 0 indicates that no auth
   * is present on the request.
   */
  unsigned char auth_size;
  /**
   * The authentication data included in the request. Only the first auth_size
   * bytes are meaningful.
   */
  unsigned char auth[58];
} praef_simple_join_request;

/**
 * Allocates and initialises a new simplified context.
 *
 * After this call, the create_node_object and decode_event callbacks must
 * still be configured by the caller. The notification callbacks may be
 * configured if desired. There is no way to configure the bridge callbacks.
 * Note in particular that neutralise_event_bridge always uses the default
 * implementation, which neutralises events by overwriting the event.apply
 * method as a noop, which may hamper applications storing rollback information
 * in their events.
 *
 * Lifetime analysis:
 *
 *   *'a praef_simple_context praef_simple_new(
 *     *mut 'a T userdata,
 *     *mut 'a praef_message_bus bus,
 *     *const 'a PraefNetworkIdentifierPair_t self);
 *
 * Arguments not documented here are as per praef_system_new().
 *
 * @param userdata User data passed into every callback.
 * @param self The network identifier used for the local node. Note that this
 * is not the simplified form. However, opaque instances can be obtained from
 * praef_udp_message_bus or praef_virtual_bus.
 * @return The new simplified context, or NULL if the object could not be
 * constructed.
 * @see praef_system_new()
 */
praef_simple_context* praef_simple_new(
  praef_userdata userdata,
  praef_message_bus* bus,
  const PraefNetworkIdentifierPair_t* self,
  unsigned std_latency,
  praef_system_profile profile,
  praef_system_ip_version ip_version,
  praef_system_network_locality net_locality,
  unsigned mtu);

/**
 * Frees the given context and all memory it owns.
 *
 * Lifetime analysis:
 *
 * void praef_simple_delete(praef_simple_context context);
 */
void praef_simple_delete(praef_simple_context* context);

/**
 * Returns the system underlying the given simple context.
 *
 * This is used for calling all the praef_system_*() functions other than new
 * and delete.
 *
 * Lifetime analysis:
 *
 * *'a mut praef_system praef_simple_get_system(
 *   *'a const praef_simple_context context);
 */
praef_system* praef_simple_get_system(const praef_simple_context* context);

/**
 * Returns the userdata object associated with the given context.
 *
 * Lifetime analysis:
 *
 * *'a mut T praef_simple_get_userdata(*'a const praef_simple_context context);
 */
void* praef_simple_get_userdata(const praef_simple_context* context);

/**
 * Type for "drop" functions passed to the simplified API.
 *
 * A drop function takes a block of memory and releases any resources owned by
 * it, but does not free the block itself.
 */
typedef void (*praef_simple_drop_t)(void*);

/**
 * Type for the object.step method as exposed by the simplified API.
 *
 * @see praef_object_step_t
 * @param object The pointer to the user-level object.
 * @param id The id of object.
 * @param context The containing simplified context.
 */
typedef void (*praef_simple_object_step_t)(
  void* object, praef_object_id id,
  const praef_simple_context* context);

/**
 * Type for the object.rewind method as exposed by the simplified API.
 *
 * @see praef_object_rewind_t
 * @param object The pointer to the user-level object.
 * @param id The id of object.
 * @param instant The instant to which to rewind.
 */
typedef void (*praef_simple_object_rewind_t)(
  void* object, praef_object_id id, praef_instant instant);

/**
 * Type for the event.apply method as exposed by the simplified API.
 *
 * @see praef_event_apply_t
 * @param object The object the event applies to.
 * @param event The event to apply.
 * @param object_id The id of object.
 * @param instant The instant of the event.
 * @param serno The serial number of the event.
 * @param context The containing simplified context.
 */
typedef void (*praef_simple_event_apply_t)(
  void* object, const void* event,
  praef_object_id object_id, praef_instant instant,
  praef_event_serial_number serno,
  const praef_simple_context* context);

/**
 * Callback type for the create_node_object method.
 *
 * Lifetime analysis:
 *
 * (int, T) fn (&praef_simple_context context)
 *
 * @param dst Memory location for the callee to construct its object. The
 * allocated size is determined by praef_simple_cb_create_node_object(). This
 * is aligned to pointer size.
 * @param drop Function to destroy any resources held by dst (but not passing
 * dst to free() or similar), set by the callee.
 * @param step The step method for the object, set by the callee.
 * @param rewind The rewind method for the object, set by the callee.
 * @param context The simplified context.
 * @param id The id of the object to be constructed.
 * @return Whether successful. Failure implies OOM.
 * @see praef_app_create_node_object_t
 */
typedef int (*praef_simple_cb_create_node_object_t)(
  void* dst, praef_simple_drop_t* drop,
  praef_simple_object_step_t* step,
  praef_simple_object_rewind_t* rewind,
  const praef_simple_context* context, praef_object_id id);
/**
 * Sets the mandatory create_node_object callback in the given context.
 *
 * @param context The context to mutate.
 * @param callback The new callback function.
 * @param object_size The number of bytes to allocate for user objects.
 */
void praef_simple_cb_create_node_object(
  praef_simple_context* context,
  praef_simple_cb_create_node_object_t callback,
  size_t object_size);

/**
 * Callback type for the decode_event method.
 *
 * Lifetime analysis:
 *
 * Option<T> fn (&praef_simple_context context,
 *               &[u8] data);
 *
 * @param dst Destination memory for the callee to construct its object. The
 * allocated size is determined by praef_simple_cb_decode_event(). This is
 * aligned to pointer size.
 * @param drop Function to destroy any resources held by dst (but not passing
 * dst to free() or similar), set by the callee.
 * @param apply The apply method for the event, set by the callee.
 * @param context The simplified context.
 * @param instant The instant of the event.
 * @param object The id of the target object.
 * @param serno The serial number of the event.
 * @param data Raw data for the event to be decoded.
 * @param sz The size of data, in bytes.
 * @return Whether successful. Failure implies that data was malformed; if an
 * internal error occurred, call praef_system_oom().
 * @see praef_app_decode_event_t
 */
typedef int (*praef_simple_cb_decode_event_t)(
  void* dst, praef_simple_drop_t* drop,
  praef_simple_event_apply_t* apply,
  const praef_simple_context* context,
  praef_instant instant, praef_object_id object,
  praef_event_serial_number serno,
  const void* data, size_t sz);
/**
 * Sets the mandatory decode_event callback in the given context.
 *
 * @param context The context to mutate.
 * @param callback The new callback function.
 * @param event_size The number of bytes to allocate for user events.
 */
void praef_simple_cb_decode_event(
  praef_simple_context* context,
  praef_simple_cb_decode_event_t callback,
  size_t event_size);

/**
 * Callback to test whether the authentication on the given join request is
 * valid.
 *
 * @param context The containing simplified context.
 * @param request The flattened view of the incoming request.
 * @return Non-zero if the authentication is valid.
 * @see praef_app_is_auth_valid_t
 */
typedef int (*praef_simple_cb_auth_is_valid_t)(
  const praef_simple_context* context,
  const praef_simple_join_request* request);

/**
 * Callback to generate authenticion data for a new join request.
 *
 * @param request The outgoing join request. Initially it has no authentication
 * data; the callee may add some if it so chooses.
 * @param context The containing simplified context.
 * @see praef_app_gen_auth_t
 */
typedef void (*praef_simple_cb_auth_gen_t)(
  praef_simple_join_request* request,
  const praef_simple_context* context);

/**
 * Sets the optional authentication callbacks on a simplified context. If this
 * is not called, all join requests are accepted and no authentication
 * information is provided on outgoing requests.
 *
 * @param context The context to mutate.
 * @param is_valid Callback to test whether authentication on an incoming
 * request is valid.
 * @param gen Callback to generate authentication on outgoing requests.
 */
void praef_simple_cb_auth(
  praef_simple_context* context,
  praef_simple_cb_auth_is_valid_t is_valid,
  praef_simple_cb_auth_gen_t gen);

/**
 * @see praef_app_permit_object_id_t
 */
typedef int (*praef_simple_cb_permit_object_id_t)(
  const praef_simple_context* context, praef_object_id id);
/**
 * Sets the permit_object_id callback.
 */
void praef_simple_cb_permit_object_id(
  praef_simple_context* context,
  praef_simple_cb_permit_object_id_t cb);

/**
 * @see praef_app_acquire_id_t
 */
typedef void (*praef_simple_cb_acquire_id_t)(
  const praef_simple_context* context, praef_object_id id);
/**
 * Sets the acquire_id callback.
 */
void praef_simple_cb_acquire_id(
  praef_simple_context* context,
  praef_simple_cb_acquire_id_t cb);

/**
 * @see praef_app_discover_node_t
 */
typedef void (*praef_simple_cb_discover_node_t)(
  const praef_simple_context* context,
  const praef_simple_netid_pair* netid,
  praef_object_id node_id);
/**
 * Sets the discover_node callback.
 */
void praef_simple_cb_discover_node(
  praef_simple_context* context,
  praef_simple_cb_discover_node_t cb);

/**
 * @see praef_app_remove_node_t
 */
typedef void (*praef_simple_cb_remove_node_t)(
  const praef_simple_context* context,
  praef_object_id id);
/**
 * Sets the remove_node callback.
 */
void praef_simple_cb_remove_node(
  praef_simple_context* context,
  praef_simple_cb_remove_node_t cb);

/**
 * @see praef_app_join_tree_traversed_t
 */
typedef void (*praef_simple_cb_join_tree_traversed_t)(
  const praef_simple_context* context);
/**
 * Sets the join_tree_traversed callback.
 */
void praef_simple_cb_join_tree_traversed(
  praef_simple_context* context,
  praef_simple_cb_join_tree_traversed_t cb);

/**
 * @see praef_app_ht_scan_progress_t
 */
typedef void (*praef_simple_cb_ht_scan_progress_t)(
  const praef_simple_context* context,
  unsigned numerator, unsigned denominator);
/**
 * Sets the ht_scan_progress callback.
 */
void praef_simple_cb_ht_scan_progress(
  praef_simple_context* context,
  praef_simple_cb_ht_scan_progress_t cb);

/**
 * @see praef_app_awaiting_stability_t
 */
typedef void (*praef_simple_cb_awaiting_stability_t)(
  const praef_simple_context* context,
  praef_object_id node, praef_instant systime,
  praef_instant committed, praef_instant validated);
/**
 * Sets the awaiting_stability callback.
 */
void praef_simple_cb_awaiting_stability(
  praef_simple_context* context,
  praef_simple_cb_awaiting_stability_t cb);

/**
 * @see praef_app_information_complete_t
 */
typedef void (*praef_simple_cb_information_complete_t)(
  const praef_simple_context* context);
/**
 * Sets the information_complete callback.
 */
void praef_simple_cb_information_complete(
  praef_simple_context* context,
  praef_simple_cb_information_complete_t cb);

/**
 * @see praef_app_clock_synced_t
 */
typedef void (*praef_simple_cb_clock_synced_t)(
  const praef_simple_context* context);
/**
 * Sets the clock_synced callback.
 */
void praef_simple_cb_clock_synced(
  praef_simple_context* context,
  praef_simple_cb_clock_synced_t cb);

/**
 * @see praef_app_gained_grant_t
 */
typedef void (*praef_simple_cb_gained_grant_t)(
  const praef_simple_context* context);
/**
 * Sets the gained_grant callback.
 */
void praef_simple_cb_gained_grant(
  praef_simple_context* context,
  praef_simple_cb_gained_grant_t cb);

/**
 * @see praef_app_recv_unicast_t
 */
typedef void (*praef_simple_cb_recv_unicast_t)(
  const praef_simple_context* context,
  praef_object_id from_node, praef_instant instant,
  const void* data, size_t size);
/**
 * Sets the recv_unicast callback.
 */
void praef_simple_cb_recv_unicast(
  praef_simple_context* context,
  praef_simple_cb_recv_unicast_t cb);

/**
 * @see praef_app_log_t
 */
typedef void (*praef_simple_cb_log_t)(
  const praef_simple_context* context,
  const char* message);
/**
 * Sets the log callback.
 */
void praef_simple_cb_log(
  praef_simple_context* context,
  praef_simple_cb_log_t cb);

/**
 * Callback type used to determine the optimism of an event.
 *
 * @see praef_stdsys_optimistic_events
 */
typedef unsigned (*praef_simple_cb_event_optimism_t)(
  const praef_simple_context* context,
  praef_object_id target, praef_instant when,
  praef_event_serial_number serno,
  const void* event);
/**
 * Sets the callback used to determine the optimism of events.
 *
 * By default, all events are pessimistic.
 */
void praef_simple_cb_event_optimism(
  praef_simple_context* context,
  praef_simple_cb_event_optimism_t cb);

/**
 * Callback type to determine whether to vote for events.
 *
 * @see praef_stdsys_event_vote_t
 */
typedef int (*praef_simple_cb_event_vote_t)(
  const praef_simple_context* context,
  praef_object_id target, praef_instant when,
  praef_event_serial_number serno,
  const void* event);
/**
 * Sets the callback used to determine whether to vote for events.
 *
 * @see praef_stdsys_event_vote()
 */
void praef_simple_cb_event_vote(
  praef_simple_context* context,
  praef_simple_cb_event_vote_t cb);

#endif /* LIBPRAEFECTUS_SIMPLE_H_ */
