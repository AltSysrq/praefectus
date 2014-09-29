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
#ifndef LIBPRAEFECTUS_SYSTEM_H_
#define LIBPRAEFECTUS_SYSTEM_H_

#include "common.h"
#include "object.h"
#include "event.h"
#include "message-bus.h"
#include "clock.h"
#include "messages/PraefNetworkIdentifierPair.h"
#include "messages/PraefMsgJoinRequest.h"

/**
 * A praef_system is a full assembly of all the smaller components of
 * praefectus (though the application is still responsible for wiring some of
 * these together).
 *
 * Once created and configured, the primary interface between praefectus and
 * the application is via callbacks defined in the praef_app structure.
 */
typedef struct praef_system_s praef_system;
/**
 * Defines the callback interface between praefectus and the application.
 *
 * Callbacks suffixed with _opt may be NULL to indicate that the application
 * does not wish to do anything special on the callback. In the majority of
 * cases this means that the callback effectively does nothing.
 *
 * Callbacks suffixed with _bridge can be implemented by the "standard system
 * bridge", which is suitable for the vast majority of applications.
 *
 * Note that instances of this structure do not necessarily comprise the full
 * structure if the application was built against an older ABI. Optional
 * callbacks beyond the runtime-declared end-of-struct are assumed to be NULL.
 */
typedef struct praef_app_s praef_app;

/**
 * Describes the IP version restrictions, if any, of a praef_system.
 *
 * All nodes within one system MUST use the same IP version restrictions. It is
 * the application's responsibility to ensure that any network identifier pairs
 * it passes to praefectus meet any restrictions it has enacted.
 */
typedef enum {
  /**
   * Indicates that any IP versions are permissible. This configuration should
   * be used with caution, since it could result in a system consisting of some
   * number of IPv4-only and IPv6-only hosts that have no way to communicate.
   */
  praef_siv_any,
  /**
   * Indicates that IPv4 is the only permissible IP version. Attempts to
   * communicate with the local node from another protocol version will be
   * silently ignored.
   */
  praef_siv_4only,
  /**
   * Indicates that IPv6 is the only permissible IP version. Attempts to
   * communicate with the local node from another protocol version will be
   * silently ignored.
   */
  praef_siv_6only
} praef_system_ip_version;

/**
 * Describes the network locality restrictions, if any, of a praef_system.
 *
 * The network address of a peer in praefectus is identified by one or two IP
 * addresses, referred to as intranet and Internet. An identifier with only an
 * intranet address is "local"; one with both intranet and Internet is
 * "global".
 *
 * All nodes within one system MUST use the same locality restrictions.
 */
typedef enum {
  /**
   * Indicates that arbitrary mixtures of local and global identifiers is
   * permissible. There is virtually never any reason to use this
   * configuration, as it permits pairs of local-only nodes which cannot use
   * local communication.
   */
  praef_snl_any,
  /**
   * Indicates that all network addresses shall be local-only. Attempts to
   * communicate with the local node with a global network identifier will be
   * silently ignored.
   */
  praef_snl_local,
  /**
   * Indicates that all network addresses shall be global. Attempts to
   * communicate with the local node with a local network identifier will be
   * silently ignored.
   */
  praef_snl_global
} praef_system_network_locality;

/**
 * Instructs the application to create a node-object in the underlying system
 * with the given id. In the vast majority of applications, this corresponds to
 * creating an instance of a praef_object with that id and adding it to the
 * bottom-layer praef_context.
 *
 * @param id The id of the object to create. As long as the application has
 * implemented permit_object_id_opt correctly (if required), this id is
 * guaranteed not to conflict with any existing object id.
 */
typedef void (*praef_app_create_node_object_t)(praef_app*, praef_object_id);

/**
 * Instructs the application to initialise data for a new node with the given
 * id. This is always called immediately before create_node_object().
 *
 * Unlike create_node_object(), most applications will simply let the standard
 * system bridge implement this method, hence its separation.
 *
 * @return Whether the operation succeeds. If it fails, create_node_object()
 * will not be called and the OOM flag on the system will be set.
 */
typedef int (*praef_app_create_node_t)(praef_app*, praef_object_id);

/**
 * Returns the time at which the given node gained the GRANT status. A time in
 * the future or at the current instant indicates it does not have
 * GRANT. Typically, ~0 is used in this case.
 *
 * Most applications will simply use the implementation provided by the
 * standard system bridge.
 *
 * @param id The id of the node being queried.
 * @return The time at which GRANT was gained by the node, or a future time if
 * the node does not have GRANT.
 */
typedef praef_instant (*praef_app_get_node_grant_t)(
  praef_app*, praef_object_id);

/**
 * Returns the time at which the given node gained the DENY status. A time in
 * the future or at the current instant indicates it does not have
 * DENY. Typically, ~0 is used in this case.
 *
 * Most applications will simply use the implementation provided by the
 * standard system bridge.
 *
 * @param id The id of the node being queried.
 * @return The time at which DENY was gained by the node, or a future time if
 * the node does not have DENY.
 */
typedef praef_instant (*praef_app_get_node_deny_t)(
  praef_app*, praef_object_id);

/**
 * Requests the application to decode the given byte array into an
 * application-specific event.
 *
 * @param instant The instant of the event to be created.
 * @param object The target of the event to be created.
 * @param sn The serial number of the event to be created.
 * @param data The input data from which the event is to be decoded.
 * @param sz The number of bytes in the data buffer.
 * @return The decoded event. Ownership of the return value transfers to the
 * caller. If the data is malformed, return NULL. If an internal error occurs
 * attempting to decode the data, return NULL and arrange to destroy the
 * system.
 */
typedef praef_event* (*praef_app_decode_event_t)(
  praef_app*, praef_instant instant, praef_object_id object,
  praef_event_serial_number sn,
  const void* data, size_t sz);

/**
 * Inserts an event into the underlying system. The event given was produced by
 * a call to decode_event().
 *
 * The standard system bridge provides a reasonable implementation of this
 * function for most applications. It is guaranteed that the event does not
 * conflict with another existing event at the same level of abstraction.
 *
 * @param event An event (produced by decode_event()) to insert into the lower
 * system. Ownership transfers to the callee; however, the callee may not
 * destroy the event until the system is destroyed.
 * @see praef_app_neutralise_event_t
 */
typedef void (*praef_app_insert_event_t)(praef_app*, praef_event* event);

/**
 * Neutralises an event in-place in the underlying system. This is necessary
 * when a node violates the protocol and creates conflicting events.
 *
 * For reasons of consistency, the event itself must continue to exist, so true
 * redaction is impossible. A typical implementation (one provided by the
 * standard system bridge) is to roll the bottom praef_context back to the time
 * of the event, and then set the apply() function to one that does
 * nothing. This may not work for all applications, however, particularly those
 * where object rollback is dependent on the events themselves.
 *
 * @param event An event that was formerly passed into insert_event_bridge().
 */
typedef void (*praef_app_neutralise_event_t)(praef_app*, praef_event* event);

/**
 * Inserts a chmod into the lower layer of the system. This corresponds to
 * praef_metatransactor_chmod().
 *
 * The vast majority of applications will simply use the implementation
 * provided by the standard system bridge.
 *
 * @param target The node to be affected by this chmod. This is guaranteed to
 * correspond to an already-existing node.
 * @param voter The node voting to execute this chmod. This is guaranteed to
 * correspond to an already-existing node.
 * @param mask A mask with one bit set indicating which status bit is to be
 * modified.
 * @param when The instant at which the proposed change is to take effect.
 */
typedef void (*praef_app_chmod_t)(praef_app*, praef_object_id target,
                                  praef_object_id voter, unsigned mask,
                                  praef_instant when);
/**
 * Returns whether a chmod with the given parameters (as per
 * praef_app_chmod_t()) has already been added to the underlying system. This
 * corresponds to praef_metatransactor_has_chmod().
 *
 * The vast majority of applications will simply use the implementation
 * provided by the standard system bridge.
 */
typedef int (*praef_app_has_chmod_t)(praef_app*, praef_object_id target,
                                     praef_object_id voter, unsigned mask,
                                     praef_instant when);

/**
 * Inserts an event vote into the lower layer of the system. This corresponds
 * to praef_transactor_votefor().
 *
 * The praef_system handles filtering out duplicate votes.
 *
 * The vast majority of applications will sipmly use the implementation
 * provided by the standard system bridge.
 *
 * @param voter The node which is casting the vote.
 * @param object The object element of the event identifier triple being
 * affirmed.
 * @param instant The instant element of the event identifer triple being
 * affirmed.
 * @param serial_number The serial number element of the event identifier
 * triple being affirmed.
 */
typedef void (*praef_app_vote_t)(praef_app*,
                                 praef_object_id voter,
                                 praef_object_id object,
                                 praef_instant instant,
                                 praef_event_serial_number serial_number);

/**
 * Advances the state of the lower layer of the system by the given number of
 * ticks.
 *
 * Many applications will simply wish to use the implementation provided by the
 * standard system bridge.
 *
 * This will not be called when a system has not at least reached the
 * clock-synchronisation phase of connection setup.
 *
 * @param delta The number of steps to advance. This may be zero; in such a
 * case, the application must still ensure that it has reached a consistent
 * state before returning. The application SHOULD consider having some maximum
 * delta it is willing to process, as advancing, eg, 3 billion steps into the
 * future will usually take an unacceptably large amount of time and is likely
 * indicative of an attack.
 * @see praef_system_conf_max_advance_per_frame()
 */
typedef void (*praef_app_advance_t)(praef_app*, unsigned delta);

/**
 * Inquires the application as to whether the authentication on the given join
 * request is valid.
 *
 * The response from this call MUST be consistent for all nodes in the system,
 * regardless of current state.
 *
 * The default implementation if this callback is not provided is to consider
 * all authentication valid.
 *
 * @return Whether the join request is considered by the application to have
 * valid authentication.
 */
typedef int (*praef_app_is_auth_valid_t)(
  praef_app*, const PraefMsgJoinRequest_t*);

/**
 * Generates application-specific authentication data to be used with the given
 * join request.
 *
 * @param req The request that may need authentication data. Its auth field is
 * initially NULL.
 * @param auth_data An OCTET STRING with a 64-byte data array. If the
 * application wishes to use authentication, it should fill this data array in
 * and set the size appropriately, then point req->data at auth_data.
 */
typedef void (*praef_app_gen_auth_t)(
  praef_app*, PraefMsgJoinRequest_t* req,
  OCTET_STRING_t* auth_data);

/**
 * Inquires the application as to whether the given object identifier is
 * permissible to be used by a node.
 *
 * Many applications will have one or more "well-known" object identifiers that
 * are not maintained by any node, but rather represent "environmental" state
 * independent of any node. (For example, in a game of Pong, the ball would
 * likely have its own id.) This callback allows the application to reserve
 * such ids for its own use.
 *
 * If an id that would be generated is reserved by the application, praefectus
 * will try the next even-numbered identifier, until this call returns true.
 *
 * Note that this MUST necessarily be completely stateless. A praefectus system
 * will diverge if any two nodes produce different responses for this call.
 *
 * The assumed implementation if this callback is not provided is to accept all
 * identifiers.
 *
 * @param id The object identifier the system would like to use for a new
 * node/object pair.
 * @return Whether the application permits node-object usage of the given
 * identifier.
 */
typedef int (*praef_app_permit_object_id_t)(praef_app*, praef_object_id id);

/**
 * Notifies the application that the local node has acquired a node id.
 *
 * @param id The new id of the local node.
 */
typedef void (*praef_app_acquire_id_t)(praef_app*, praef_object_id id);

/**
 * Notifies the application that a new external node has come into existence.
 */
typedef void (*praef_app_discover_node_t)(
  praef_app*, const PraefNetworkIdentifierPair_t*, praef_object_id);

/**
 * Notifies the application that the route to the given existing external node
 * is being terminated.
 */
typedef void (*praef_app_remove_node_t)(praef_app*, praef_object_id);

/**
 * Notifies the application that traversal of the full node-join tree is
 * believed to be complete. At this point the local node will begin connecting
 * to other nodes in the system and actively synchronising with them.
 *
 * This is only ever called once at some point after a call to
 * praef_system_connect().
 */
typedef void (*praef_app_join_tree_traversed_t)(praef_app*);

/**
 * Notifies the application of the approximate current progress of the hash
 * tree scan process, which occurs during joining after the join tree has been
 * traversed. The progress is reported in terms of a fraction. The hash tree
 * scan is fully complete when this is called with numerator==denominator.
 *
 * This may be called even though the progress has not changed since the
 * previous call, and may even report a lower progress than the previous call
 * in uncommon cases.
 *
 * @param numerator The numerator of current progress. This is guaranteed
 * to be <= denominator.
 * @param denominator The denominator of the current progress. This is
 * guaranteed to be non-zero.
 */
typedef void (*praef_app_ht_scan_progress_t)(
  praef_app*, unsigned numerator, unsigned denominator);

/**
 * Notifies the application that the local node now believes it has all
 * information necessary to interoperate with other live nodes.
 *
 * This will only be called after a call to praef_system_connect().
 */
typedef void (*praef_app_information_complete_t)(praef_app*);

/**
 * Notifies the application that the local node's clock is believed to be
 * reasonably synchronised with the rest of the system, and that the
 * application's state has been advanced up to that point.
 */
typedef void (*praef_app_clock_synced_t)(praef_app*);

/**
 * Notifies the application that the local node has tenatively received the
 * GRANT status for the first time. This is usually permanent, though in
 * certain circumstances the status could be temporarily or even permanently
 * lost after this notification.
 *
 * Generally, the application can view this notification as an intication that
 * the system is fully set up and ready for full use.
 */
typedef void (*praef_app_gained_grant_t)(praef_app*);

/**
 * Notifies the application that it has received an application-defined unicast
 * message.
 *
 * How the application chooses to handle invalid application-defined unicast
 * messages is entirely up to the application. No mechanism is provided for the
 * application to inform libpraefectus about invalid messages, for example.
 *
 * If no implementation for this function is given, all incoming
 * application-defined unicast messages are discarded.
 *
 * @param from_node The node that signed the packet containing this
 * message. Note that there is no guarantee that that node actually sent the
 * message to *this* node. The application MUST gracefully handle receiving
 * unicast messages intended for another node.
 * @param instant The instant attached to the packet containing the
 * message. This corresponds to the time of from_node when it created the
 * message.
 * @param data The contents of the application-defined message.
 * @param size The number of bytes in the data buffer.
 */
typedef void (*praef_app_recv_unicast_t)(praef_app*, praef_object_id from_node,
                                         praef_instant instant,
                                         const void* data, size_t size);

/**
 * Notifies the application of a developer-targetted log message produced by
 * the system.
 */
typedef void (*praef_app_log_t)(praef_app*, const char*);

struct praef_app_s {
  /**
   * Equal to the compile-time value of sizeof(praef_app). (This allows new
   * fields to be added without breaking binary compatibility).
   */
  size_t size;

  praef_app_create_node_object_t create_node_object;
  praef_app_decode_event_t decode_event;

  praef_app_create_node_t create_node_bridge;
  praef_app_get_node_grant_t get_node_grant_bridge;
  praef_app_get_node_deny_t get_node_deny_bridge;
  praef_app_insert_event_t insert_event_bridge;
  praef_app_neutralise_event_t neutralise_event_bridge;
  praef_app_chmod_t chmod_bridge;
  praef_app_has_chmod_t has_chmod_bridge;
  praef_app_vote_t vote_bridge;
  praef_app_advance_t advance_bridge;

  /* Optional control callbacks */
  praef_app_permit_object_id_t permit_object_id_opt;
  praef_app_is_auth_valid_t is_auth_valid_opt;
  praef_app_gen_auth_t gen_auth_opt;

  /* Optional notification callbacks */
  praef_app_acquire_id_t acquire_id_opt;
  praef_app_discover_node_t discover_node_opt;
  praef_app_remove_node_t remove_node_opt;
  praef_app_join_tree_traversed_t join_tree_traversed_opt;
  praef_app_ht_scan_progress_t ht_scan_progress_opt;
  praef_app_information_complete_t information_complete_opt;
  praef_app_clock_synced_t clock_synced_opt;
  praef_app_gained_grant_t gained_grant_opt;
  praef_app_log_t log_opt;

  /* Optional application-defined-message callbacks */
  praef_app_recv_unicast_t recv_unicast_opt;
};

/**
 * Describes the status of a praef_system. Any status that does not test as
 * false can be considered abnormal, but whether such an abnormality is
 * permanent depends on the particular status.
 *
 * In cases where more than one status is considered relevant, the most severe
 * one is generally used.
 */
typedef enum {
  /**
   * Indicates that the system is normally connected.
   */
  praef_ss_ok = 0,
  /**
   * Indicates that the local node has not received an identifier yet, and so
   * is unable to do terribly much. This status becomes cleared upon
   * bootstrapping or successful connection. In the latter case, the status can
   * be expected to tranfer to pending_grant.
   *
   * If this status remains for too long (as defined by the application) after
   * a connect attempt, the application should destroy this system and retry
   * connection with a new one.
   */
  praef_ss_anonymous,
  /**
   * Indicates that the local node has not yet received the GRANT status, and
   * so cannot yet participate in the system .
   *
   * If this status remains for too long (as defined by the application), the
   * application should destroy this system and retry connection with a new
   * one.
   */
  praef_ss_pending_grant,
  /**
   * Indicates that the local node has entered a state in which it is
   * attempting to DENY more than half of the nodes in the system which are
   * currently alive. This usually indicates a permanent and fatal network
   * partition. (The most common cause arising from the local host losing
   * network/Internet access.) Applications may wish to wait a short while to
   * see if the status clears itself, which can happen if new nodes are
   * introduced before any vote passes.
   */
  praef_ss_partitioned,
  /**
   * Indicates that the local node has gained the DENY status, preventing it
   * from taking further meaningful action. This is almost always a permanent
   * and fatal condition, though applications may wish to wait a short while to
   * see if it gets cleared, which can happen due to retroactive status changes
   * of other nodes in some circumstances.
   */
  praef_ss_kicked,
  /**
   * Indicates that the system ran out of memory while attempting to carry on
   * normal operations. This means that the local node is no longer able to
   * properly follow the protocol, and thus is a fatal error. Upon seeing this
   * status, the application should clean up and destroy the system.
   */
  praef_ss_oom,
  /**
   * Indicates that the end of time has been reached, prevting the system from
   * making any further progress. This is a permanent and fatal condition,
   * which furthermore prevents graceful disconnects from being meaningful.
   *
   * Most applications will never encounter this condition unless a malicious
   * node severely skews the clock while it has strong control over it (and if
   * the application accepts such great time advancement).
   *
   * Note that for the sake of being conservative about correctness, the
   * threshold of "the end of time" is actually at instant 0x80000000 instead
   * of 0xFFFFFFFF.
   */
  praef_ss_overflow,
  /**
   * Indicates that the local node experienced a fatal id collision with
   * another node. This status is always permanent and fatal. Graceful
   * disconnect may or may not be possible.
   */
  praef_ss_collision
} praef_system_status;

/**
 * Defines the strategy for calculating default configuration values from the
 * standard latency of the system.
 */
typedef enum {
  /**
   * Specifies to make the system as strict as possible. Any time-based
   * configuration that can meaningfully be zero on a latent network is set to
   * zero. This will result in amplified network latency effects (typically
   * quadruple that of a one-way trip), but makes it impossible for malicious
   * nodes to gain unfair advance knowledge.
   */
  praef_sp_strict,
  /**
   * Specifies to make the system more permissive, setting certain non-critical
   * values according to the standard latency. This substantially reduces
   * latency effects (in good cases, bringing them down to that of a single
   * one-way trip); however, malicious nodes whose true latency to the local
   * host is under the standard latency may gain unfair advance knowledge.
   *
   * This is usually the best choice for applications driven by real-time human
   * input, especially if their standard latency is at or below general human
   * reaction time.
   */
  praef_sp_lax
} praef_system_profile;

/**
 * Constructs a new, empty, unconnected praef_system.
 *
 * @param app The application callback interface. The application must ensure
 * that this pointer remains valid for the lifetime of the system.
 * @param bus The message bus to use for communication. The application must
 * ensure that this pointer remains valid for the lifetime of the system.
 * @param self The network identifier of the local node.
 * @param std_latency The "standard latency" of the underlying network, in
 * terms of instants. This is used (along with profile) to set the default
 * configuration of the system. This should be the latency that the application
 * is designed to most commonly experience, rather than the maximum value it
 * expects to. For example, an application operating over the Internet at 64
 * instants per real second could reasonably use a standard latency of 8 (125
 * ms). A value of zero is not reasonable even on zero-latency networks, as
 * a minimum latency of one instant is introduced by the nature of the
 * message-bus abstraction.
 * @param profile The profile to use to calculate default configuration based
 * on std_latency.
 * @param ip_version The IP version restrictions on the system, if any.
 * @param net_locality The network locality restrictions on the system, if
 * any.
 * @param mtu The MTU for message encoders. This needs to be at least
 * PRAEF_HLMSG_MTU_MIN+8.
 * @return The new system, or NULL if insufficient memory was available.
 */
praef_system* praef_system_new(praef_app* app,
                               praef_message_bus* bus,
                               const PraefNetworkIdentifierPair_t* self,
                               unsigned std_latency,
                               praef_system_profile profile,
                               praef_system_ip_version ip_version,
                               praef_system_network_locality net_locality,
                               unsigned mtu);

/**
 * Frees all memory held by the given praef_system.
 *
 * If connected to other nodes, this is a *graceless*
 * disconnection. Applications are recommended to gracefully disconnect with
 * praef_system_disconnect() if doing so when the status of the system is
 * "ok".
 */
void praef_system_delete(praef_system*);

/**
 * "Connects" this system by making the local node the bootstrap node. This
 * only makes sense for a newly-created praef_system.
 */
void praef_system_bootstrap(praef_system*);
/**
 * Connects this system by executing the node-joining protocol with the
 * existing node at the given network address. This is asynchronous (ie,
 * proceeds during successive calls to praef_system_advance()). If it succeeds,
 * the system will transition away from the "anonymous" status.
 *
 * @param target The network identifier to use to initiate the connection. The
 * system object takes ownership of this pointer.
 */
void praef_system_connect(praef_system*,
                          const PraefNetworkIdentifierPair_t* target);

/**
 * Initiates a graceful disconnect from the system. The graceful disconnect
 * will usually succeed if praef_system_advance() is called at least once
 * after this call, though applications may wish to wait for the status to
 * cease to be "ok" to ensure disconnects are as graceful as possible.
 */
void praef_system_disconnect(praef_system*);

/**
 * Advances the system the given number of instants forward.
 *
 * @return The current status of the system.
 */
praef_system_status praef_system_advance(praef_system*, unsigned);

/**
 * Adds a new (encoded) event to the system, which implicitly applies to the
 * local node's object. This call is not meaningful if the local node does not
 * yet have an id.
 *
 * @param data The encoded form of the event, as decodable by the application's
 * decode_event().
 * @param size The size of the data buffer, in bytes.
 * @return Whether the operation succeeds.
 */
int praef_system_add_event(praef_system*, const void* data, size_t size);

/**
 * Adds a new vote-for event to the system, indicating the local node's
 * acceptance of an event identified by the given triple.
 */
int praef_system_vote_event(praef_system*, praef_object_id,
                            praef_instant, praef_event_serial_number);

/**
 * Sends an application-defined unicast message to the specified node. No
 * reliablity or synchronicity mechanism is provided through this call;
 * messages it sends can arrive in any order and any number of times. If the
 * application wishes to know if or when the target receives the message, it
 * must implement such a mechanism itself.
 *
 * It is also possible for the receiver to forward the message to another node
 * in the system, which will make the final receiver believe the local node
 * sent it directly to that node. If this is a problem, the application should
 * include destination node ids in its messages and validate them itself.
 *
 * @param target The id of the node that is to receive this message.
 * @param data The encoded form of the message, as decodable by the
 * application's recv_unicast_opt().
 * @param size The size of the data buffer, in bytes.
 * @return Whether the operation succeeds. ("Success" meaning that nothing
 * surprising, such as out-of-memory, occurred when attempting to carry out the
 * request.)
 */
int praef_system_send_unicast(praef_system*, praef_object_id target,
                              const void* data, size_t size);

/**
 * Returns the global clock maintained by the given praef_system.
 */
const praef_clock* praef_system_get_clock(praef_system*);

/**
 * Sets the Out-of-Memory flag on the given praef_system. All further calls to
 * praef_system_advance() will return praef_ss_oom.
 */
void praef_system_oom(praef_system*);

/**
 * Configures the obsolescence interval of the clock on the given system.
 *
 * The default is 5*std_latency.
 *
 * @see praef_clock::obsolescence_interval
 */
void praef_system_conf_clock_obsolescence_interval(praef_system*, unsigned);
/**
 * Configures the tolerance of the clock on the given system.
 *
 * The default is std_latency.
 *
 * @see praef_clock::tolerance
 */
void praef_system_conf_clock_tolerance(praef_system*, unsigned);

/**
 * Configures the commit interval, in instants, of the given system.
 *
 * Smaller values reduce latency effects but increase bandwidth usage.
 *
 * The default is std_latency/2, or 1, whichever is greater.
 */
void praef_system_conf_commit_interval(praef_system*, unsigned);
/**
 * Configures the maximum commit lag, in instants, of the given system.
 *
 * The local node will begin voting to DENY nodes with GRANT whose commit
 * threshold lags behind the current time by more than this number of
 * instants. Since commits are simply single messages, fairly low latencies can
 * usually be expected; however, this does need to be at least
 * praef_system_conf_public_visibility_lag() +
 * praef_system_conf_linear_ack_interval() +
 * praef_system_conf_commit_interval() in order to correctly recover from
 * packet loss. Larger values are more forgiving of high-latency nodes and
 * packet loss, but increase the possible latency effects proportionally.
 *
 * The default is std_latency*32.
 */
void praef_system_conf_max_commit_lag(praef_system*, unsigned);
/**
 * Configures the maximum validated lag, in instants, of the given system.
 *
 * The local node will begin voting to DENY nodes with GRANT whose validated
 * threshold lags behind the current time by more than this number of
 * instants. Since many messages may be part of a commit, greater latencies
 * than commits can usually be expected. Larger values are more forgiving of
 * high-latency nodes, but increase the possible latency effects
 * proportionally.
 *
 * It does not make sense for this to be less than
 * praef_system_conf_max_commit_lag().
 *
 * The default is std_latency*64.
 */
void praef_system_conf_max_validated_lag(praef_system*, unsigned);
/**
 * Configures the laxness of the commited threshold of a node as it applies to
 * that node's ability to see committed-redistributable messages.
 *
 * This parameter is designed to mitigate latency effects incurred by other
 * nodes' networks. It allows nodes to see an additional number of instants
 * equal to this value beyond the commit threshold, even after adjusting for
 * the local node's estimated intrinsic latency. Larger values reduce latency
 * effects; if the sum of this value and the local node's estimated intrinsic
 * latency (after adjustment via
 * praef_system_conf_self_commit_lag_compensation()) is greater than or equal
 * to the true latency between two nodes, the effects of latency from the local
 * node to the destination node are completely reduced to that of a one-way
 * trip in the absence of packet loss.
 *
 * Note that any non-zero value for this field makes it possible for other
 * nodes to see beyond their commit threshold, which could permit them to make
 * decisions based upon information that should be in the future for them.
 *
 * Typically, this should be set to half the expected one-way latency plus any
 * delay that could be incurred by praef_system_conf_commit_interval(),
 * provided that the advance knowledge nodes could gain is not useful in that
 * time interval.
 *
 * For the lax profile, this defaults to std_latency. For the strict profile,
 * it defaults to 0.
 */
void praef_system_conf_commit_lag_laxness(praef_system*, unsigned);
/**
 * Configures a fraction by which to multiply the estimated latency incurred by
 * the local node's network in order to compensate for commit lag.
 *
 * The local node maintains an estimate of how much latency its own local
 * network incurs when talking to other nodes. This can be thought of as
 * one-quarter the minimum round-trip time to any other node. This estimate is
 * multiplied by some fraction to determine how much to adjust the message
 * visibility for other nodes based on their commit thresholds.
 *
 * Larger values reduce the effects of latency; together with
 * praef_system_conf_commit_lag_laxness(), they can be reduced to that of a
 * one-way trip in the absence of packet loss.
 *
 * Note that any non-zero value makes it possible for other nodes to see beyond
 * their commit threshold. However, assuming that no node has an exceptionally
 * low latency to the local node which it is able to hide, any node wishing to
 * do so must necessarily delay comitting for the true one-way latency between
 * the nodes, which still causes it to be deprived of further information. In
 * most cases, this is a greater disadvantage than anything provided by the
 * future information.
 *
 * For the vast majority of applications, 1/1 is reasonable, though values up
 * to 2/1 may occasionally be desirable to further mitigate latency. The
 * decesion of what this value should be should be based upon how useful
 * future information is versus the additional latency that would be incurred
 * in exchange for acquiring it.
 *
 * For the lax profile, this defaults to 1/1. For the strict profile, it
 * defaults to 0/1.
 *
 * @param numerator The numerator of the fraction. This must be below 65536.
 * @param denominator The denominator of the fraction. This must must be
 * non-zero.
 */
void praef_system_conf_self_commit_lag_compensation(
  praef_system*, unsigned numerator, unsigned denominator);
/**
 * Controls the threshold beyond which all information is assumed public.
 *
 * Larger values reduce the possibility of using side-channels to gain
 * information without committing, but reduces the synchronicity of a new node
 * immediately after connecting.
 *
 * The default is 4 * std_latency.
 */
void praef_system_conf_public_visibility_lag(
  praef_system*, unsigned lag);
/**
 * Configures the number of instants in which all live nodes are maintaining
 * their commitments that the local node will wait before requesting GRANT
 * permissions.
 *
 * This value directly adds to the total connection time. Larger values reduce
 * the probability that a new node will fumble around and incorrectly deem
 * another node dead immediately after it gains GRANT.
 *
 * The default is 4 * std_latency.
 */
void praef_system_conf_stability_wait(praef_system*, unsigned);
/**
 * Configures the interval upon which queries to the node join tree are
 * retried.
 *
 * Smaller values will allow the traversal of the join tree to recover more
 * quickly from dropped packets, reducing the time it takes to become a
 * fully-functional member of a system, but may make the network noisier.
 *
 * The default is std_latency.
 */
void praef_system_conf_join_tree_query_interval(
  praef_system*, unsigned interval);
/**
 * Controls the minimum interval between the acceptance of new nodes into the
 * system by *this node*. Attempts to join the system which arrive before this
 * interval has expired since the last accept are silently ignored.
 *
 * This value simply makes the system more DoS-resistant, and only controls the
 * behaviour of the local node. Other nodes in the system can still potentially
 * accept nodes into the system at an arbitrary rate.
 *
 * The only way to strongly protect an application from accept floods is to
 * define an authentication mechanism which makes it difficult or impossible to
 * generate large numbers of valid accepts. For example, one could employ a
 * centralised server that is the sole source of authentication information,
 * and either has a limited rate at which it will generate such information, or
 * is aware of the number of nodes in a system and will refuse to exceed that
 * number.
 *
 * The default is std_latency*8.
 */
void praef_system_conf_accept_interval(
  praef_system*, unsigned interval);
/**
 * Controls the approximate maximum node count in the system. The local node
 * will not accept join requests when the number of live nodes is greater than
 * or equal to this value.
 *
 * Note that there is nothing preventing other nodes already in the system from
 * blowing past this limit if they choose to do so. Furthermore, even when all
 * nodes are cooperating with the same limit, the limit can still be exceeded
 * due to the asynchronous nature of the network. Therefore, applications MUST
 * be able to handle systems which exceed the node count limit.
 *
 * The default is ~0 (ie, effectively no limit).
 */
void praef_system_conf_max_live_nodes(
  praef_system*, unsigned max);
/**
 * Controls the maximum number of responses that will be produced in response
 * to a hash-tree-range query.
 *
 * Larger values *may* make the joining of new nodes faster, provided bandwidth
 * constraints do not cause packets to be dropped. The approximate outgoing
 * bandwidth requirement is generally approximately
 *
 *   mtu * ht_range_max / round_trip_latency
 *
 * The default value is 64. On a system with the minimal MTU of 307 bytes, and
 * a network with 100ms round-trip latency, this consumes up to 191kB/sec (or
 * 1.5 Mbps) of bandwidth (not including IP overhead and such) while a node is
 * joining per concurrent range query (see
 * praef_system_conf_ht_scan_concurrency()).
 */
void praef_system_conf_ht_range_max(praef_system*, unsigned);
/**
 * Controls the interval at which hash-tree-range queries are retried.
 *
 * Larger values make node joining faster in the presence of packet loss, but
 * may cause excessive redundant traffic if too low.
 *
 * The default is std_latency*4.
 */
void praef_system_conf_ht_range_query_interval(praef_system*, unsigned);
/**
 * Controls the redundancy level used for hash tree scans when joining a
 * system.
 *
 * When possible, at least this many nodes will be queried for the same ranges
 * in the hash tree, though no node will be queried for the same range more
 * than once. Join time increases linearly with this value, but the certainty
 * that all messages are received from the scan increases as well. This may
 * also increase bandwidth usage, though that can be more precicely controlled
 * with praef_system_conf_ht_range_query_interval(),
 * praef_system_conf_ht_range_max(), and
 * praef_system_conf_ht_scan_concurrency().
 *
 * The default is 2.
 */
void praef_system_conf_ht_scan_redundancy(praef_system*, unsigned);
/**
 * Controls the maximum concurrency with which the local node will perform
 * range queries during the hash tree scan. Note that the local node never runs
 * more than one conncurrent range query against any other single node.
 *
 * This is a direct linear trade-off between bandwidth usage and the time it
 * takes to join. (Obviously, once bandwidth limits are hit, increasing the
 * value further will only reduce performance due to packet loss.) This value
 * must be less than 255. A value of zero is permissible, but is generally not
 * a good idea, as it will cause the hash tree scan step to be skipped.
 *
 * The default is 3.
 */
void praef_system_conf_ht_scan_concurrency(praef_system*, unsigned char);
/**
 * Controls the intervals at which hash tree snapshots are taken.
 *
 * Specifically, one hash tree snapshot will be taken at every instant which is
 * evenly divisible by this value. Smaller values (more frequent snapshots)
 * will allow nodes to more precicely synchronise their hash trees in the
 * presence of minor clock skew and latency jitter, at the cost of lower
 * tolerance to larger clock skew and latency.
 *
 * It is highly advisable for all nodes in a system to use the same value for
 * this configuration.
 *
 * The default is std_latency.
 *
 * The effect of passing a value of 0 is undefined.
 */
void praef_system_conf_ht_snapshot_interval(praef_system*, unsigned);
/**
 * Controls the maximum number of hash tree snapshots that will be maintained.
 *
 * Larger values counteract the downsides of more frequent snapshots (as
 * documented in praef_system_conf_ht_snapshot_interval()) in exchange for
 * somewhat greater memory and CPU usage.
 *
 * The default is 64. Invoking this function will purge any existing snapshots
 * if it succeeds, so it should generally only be used before the system is
 * connected.
 *
 * @return Whether adjusting the configuration succeeds. If 0 is returned, the
 * configuration is unchanged and no snapshots were purged.
 */
int praef_system_conf_ht_num_snapshots(praef_system*, unsigned);
/**
 * Controls the interval at which queries against the root hash tree directory
 * of each node are performed.
 *
 * Smaller values will reduce the effects of packet loss (in particular
 * momentary network partitions) by resynchronising the hash trees more
 * quickly, at the cost of greater bandwidth usage. Note that this is a
 * particularly inefficient way of recovering from packet loss; it is intended
 * as a back-up for when the much faster and lighter sequence-number-based
 * system fails to recover packets. Frequent queries can consume substantial
 * amounts of redundant bandwidth.
 *
 * The default is std_latency*16.
 */
void praef_system_conf_ht_root_query_interval(praef_system*, unsigned);
/**
 * Controls the time offset for hash tree queries.
 *
 * Specifically, any queries against another node's hash tree is back-dated by
 * this amount. Smaller values permit recovering from momentary network
 * partitions more quickly; however, very small values can incur potentially
 * large amounts of wasted bandwidth in cases of clock skew.
 *
 * The default is std_latency*4.
 */
void praef_system_conf_ht_root_query_offset(praef_system*, unsigned);
/**
 * Controls the interval at which Route messages are sent to new nodes that
 * have received neither the GRANT nor the DENY status.
 *
 * Lower values will allow new nodes to discover which nodes in the system are
 * still active, which makes their connection faster, and will generally reduce
 * load on the initial node to which they connect, as the hash tree scan will
 * be better distributed. The only cost is bandwidth, which is fairly low.
 *
 * The default is 4*std_latency.
 */
void praef_system_conf_ungranted_route_interval(praef_system*, unsigned);
/**
 * Controls the interval at which Route messages are sent to nodes which have
 * already received the GRANT status and have not received the DENY status.
 *
 * Lower values increase the probability of new nodes surviving untimely
 * network partitions and reduce the window of the initial connection target
 * being a single point of failure, but otherwise have little effect. As with
 * praef_system_conf_ungranted_route_interval(), the only real cost is
 * bandwidth.
 *
 * The default is 32*std_latency.
 */
void praef_system_conf_granted_route_interval(praef_system*, unsigned);
/**
 * Controls the interval at which new Ping messages will be sent to other
 * nodes.
 *
 * Lower values yield more accurate short-term estimates of latency, thus
 * adapting more quickly to changes in the behaviour of the network. However,
 * this also acts as a cap on the maximum network latency between any two
 * nodes, beyond which both nodes will switch to a negative disposition towards
 * each other.
 *
 * The default is 16*std_latency.
 */
void praef_system_conf_ping_interval(praef_system*, unsigned);
/**
 * Controls the maximum amount of time that may elapse since receiving a valid
 * Pong response from another node before the local node will switch to a
 * negative disposition to this node. This occurs even if other traffic is
 * still being received from that node.
 *
 * This is primarily a defence against nodes deliberately not responding to
 * Pings in order to hide their latency; commit and verification lag are the
 * primary means of liveness testing.
 *
 * Lower values increase the chance of incorrectly identifying a node as dead
 * or malicious due to packet loss.
 *
 * The default is 128*std_latency, ie, up to 8 pings at the default interval
 * may be lost.
 */
void praef_system_conf_max_pong_silence(praef_system*, unsigned);
/**
 * Controls the amount of time after a node has received the DENY status that
 * the local node permanently kills its route to that node.
 *
 * When a remote node first gets killed, the local node will continue
 * maintaining communications with it for a while, in case new information
 * causes the DENY status to be retroactively removed. This reduces the
 * negative effects of such an error, though the local node will continue
 * attempting to make the DENY status permanent.
 *
 * Larger values improve the resilience to DENY revocations. However, there is
 * a (potentially substantial) cost of both bandwidth and CPU if the node
 * incorrectly or maliciously acts as if it is still alive, due to divergence
 * of hash trees and ack lists.
 *
 * The default is 256*std_latency.
 */
void praef_system_conf_route_kill_delay(praef_system*, unsigned);
/**
 * Controls the interval at which the local node will propose for itself to
 * gain the GRANT status once it feels it is ready to handle it.
 *
 * Larger values will somewhat reduce the time required to join a system in the
 * face of packet loss, at the cost of slightly greater bandwidth usage.
 *
 * The default is 16*std_latency.
 */
void praef_system_conf_propose_grant_interval(praef_system*, unsigned);
/**
 * Controls the frequency at which the local node will suggest setting the DENY
 * bit on any node it feels negatively about.
 *
 * All nodes in the system should use the same value for this configuration to
 * minimise bandwidth, though the penalty for not doing so is reasonably
 * small. Larger values increase bandwidth usage, but will somewhat reduce the
 * time needed to start DENYing another node.
 *
 * The default is 16*std_latency.
 *
 * @see praef_system_conf_vote_chmod_offset()
 */
void praef_system_conf_vote_deny_interval(praef_system*, unsigned);
/**
 * Controls the offset into the future at which GRANT/DENY suggestions produced
 * by the local node are proposed to take effect.
 *
 * All nodes in the system MUST use the same value for this configuration or
 * risk divergence. Larger values increase the probability of a status change
 * carrying when sufficient nodes are in agreement, at the cost of it taking
 * longer for the the status change to take effect.
 *
 * This setting also controls the maximum distance into the future relative to
 * the message envelope at which chmod events will be accepted.
 *
 * The default is 16*std_latency.
 */
void praef_system_conf_vote_chmod_offset(praef_system*, unsigned);
/**
 * Configures the amount of time that a new node has before it is expected to
 * begin adhering to timeliness requirements, in particular the commit lag
 * requirements.
 *
 * This necessarily needs to be greater than intervals (such as the commit
 * interval) upon which this is based.
 *
 * The default is 16*std_latency.
 */
void praef_system_conf_grace_period(praef_system*, unsigned);
/**
 * Configures the interval at which acknowledgement packets are sent to the
 * node which sent the packets they are acknowledging.
 *
 * Smaller values will improve behaviour in the face of accidental packet loss,
 * in exchange for greater bandwidth usage.
 *
 * The default is 4*std_latency.
 */
void praef_system_conf_direct_ack_interval(praef_system*, unsigned);
/**
 * Configures the interval at which acknowledgement packets are sent to nodes
 * other than the node which sent the packets they are acknowledging.
 *
 * Smaller values will improve behaviour in the face of both accidental and
 * deliberate packet loss, in exchange for greater bandwidth usage. This is
 * only a fallback mechanism which only offers partial protection from
 * deliberate packet loss; the hash tree system covers for the rest of
 * this. Indirect acks consume quadratic bandwidth per peer with respect to the
 * number of live nodes in a system.
 *
 * There is generally no reason to set this to a low value, unless the
 * application also reduces the hash tree query interval.
 *
 * The default is 16*std_latency.
 */
void praef_system_conf_indirect_ack_interval(praef_system*, unsigned);
/**
 * Configures the interval at which "linear acknowledgement" packets are sent
 * to nodes.
 *
 * Smaller values will marginally improve packet recovery from cooperating
 * nodes at the cost of some bandwidth.
 *
 * The default is 2*std_latency.
 */
void praef_system_conf_linear_ack_interval(praef_system*, unsigned);
/**
 * Configures the maximum number of packets that will be retransmitted in
 * response to a linear ack message suggestive of packet loss.
 *
 * There is a direct correlation between wasted bandwidth and this value. This
 * is only a secondary packet-recovery mechanism, so it typically should be
 * fairly small, generally about the number of messages the application expects
 * to produce in two round-trips.
 *
 * The default is 4.
 */
void praef_system_conf_linear_ack_max_xmit(praef_system*, unsigned);
/**
 * Configures the maximum number of ticks that the system will advance the
 * application layer (via praef_app_advance_t()) per frame.
 *
 * This allows the application to remain responsive while catching up to an
 * existing system that has already advanced quite far. It also provides some
 * protection from malicious nodes that deliberately skew the clock by large
 * amounts.
 *
 * The default is ~0u; ie, there is effectively no limit.
 */
void praef_system_conf_max_advance_per_frame(praef_system*, unsigned);
/**
 * Configures the maximum offset (absolute value) of an event-vote from the
 * event itself.
 *
 * This value MUST be the same across all nodes in the system, or malicious
 * nodes will be able to cause the system to diverge. Vote messages which
 * violate this constraint are discarded and cause the local node to begin
 * voting to DENY the offending node.
 *
 * Stricter values reduce the possibility for malicious nodes to change the
 * vote outcome of an event long after the event actually occurred.
 *
 * It is the application's responsibility to ensure it does not violate this
 * constraint.
 *
 * The default is ~0u (ie, unlimited), since the application needs to be fully
 * aware of this configuration itself. Virtually all applications will want to
 * set this to something more reasonable.
 */
void praef_system_conf_max_event_vote_offset(praef_system*, unsigned);

#endif /* LIBPRAEFECTUS_SYSTEM_H_ */
