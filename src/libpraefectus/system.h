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
#include "message-bus.h"
#include "messages/PraefNetworkIdentifierPair.h"

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
 * does not wish to do anything special on the callback.
 *
 * Note that instances of this structure do not necessarily comprise the full
 * structure if the application was built against an older ABI. Optional
 * callbacks beyond the runtime-declared end-of-struct are assumed to be NULL.
 */
typedef struct praef_app_s praef_app;

struct praef_app_s {
  /**
   * Equal to the compile-time value of sizeof(praef_app). (This allows new
   * fields to be added without breaking binary compatibility).
   */
  size_t size;
  /* TODO */
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
   * bootstrapping or successful connection.
   *
   * If this status remains for too long (as defined by the application) after
   * a connect attempt, the application should destroy this system and retry
   * connection with a new one.
   */
  praef_ss_anonymous,
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
   */
  praef_ss_overflow
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
 * @param mtu The MTU for message encoders. This needs to be at least
 * PRAEF_HLMSG_MTU_MIN+8.
 * @return The new system, or NULL if insufficient memory was available.
 */
praef_system* praef_system_new(praef_app* app,
                               praef_message_bus* bus,
                               const PraefNetworkIdentifierPair_t* self,
                               unsigned std_latency,
                               praef_system_profile profile,
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
 */
void praef_system_connect(praef_system*, const PraefNetworkIdentifierPair_t*);

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
 * usually be expected. Larger values are more forgiving of high-latency nodes,
 * but increase the possible latency effects proportionally.
 *
 * The default is std_latency*8.
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
 * The default is std_latency*16.
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

#endif /* LIBPRAEFECTUS_SYSTEM_H_ */
