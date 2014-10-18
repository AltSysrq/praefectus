
AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

# Recursing into asn1-support first has the nice effect of forcing everything
# there to be built first, so we don't need to go through the headache of
# getting Automake to comprehend that a task produces multiple header files.
SUBDIRS = asn1-support libpraefectus/messages udp-common .

AM_CFLAGS=-Iasn1-support -D_BSD_SOURCE \
          $(SDL_CFLAGS) $(SDLIMG_CFLAGS)
AM_LDFLAGS=$(SDL_LIBS) $(SDLIMG_LIBS)

bin_PROGRAMS = praef
noinst_PROGRAMS = nbodies/nbodies-vnet-4-ideal nbodies/nbodies-vis \
  nbodies/nbodies-vnet-8-latent-lossy
noinst_LTLIBRARIES = libpraefectus.la libasn1-support.la \
libpraefectus-messages.la nbodies/libnbodies-core.la \
libudp-common.la

praef_LDADD = libpraefectus.la
praef_SOURCES = \
praefectus/top/main.c \
praefectus/top/test-state.c \
praefectus/graphics/canvas.c \
praefectus/graphics/crt.c \
praefectus/graphics/font.c \
praefectus/graphics/fraktur.c

libpraefectus_la_SOURCES = \
libpraefectus/keccak/KeccakF-1600-opt64.c \
libpraefectus/keccak/KeccakSponge.c \
libpraefectus/object.c \
libpraefectus/event.c \
libpraefectus/context.c \
libpraefectus/transactor.c \
libpraefectus/metatransactor.c \
libpraefectus/mtt-bridge.c \
libpraefectus/std-state.c \
libpraefectus/hash-tree.c \
libpraefectus/commitment-chain.c \
libpraefectus/secure-random.c \
libpraefectus/dsa.c \
libpraefectus/virtual-bus.c \
libpraefectus/hl-msg.c \
libpraefectus/clock.c \
libpraefectus/outbox.c \
libpraefectus/ack-table.c \
libpraefectus/system-state.c \
libpraefectus/system-router.c \
libpraefectus/system-join.c \
libpraefectus/system-htm.c \
libpraefectus/system-routemgr.c \
libpraefectus/system-mod.c \
libpraefectus/system-commit.c \
libpraefectus/system-ack.c \
libpraefectus/system.c \
libpraefectus/stdsys.c

libpraefectus_la_LIBADD = \
  libpraefectus-messages.la libasn1-support.la

libpraefectus_messages_la_CFLAGS = -w -Iasn1-support
libpraefectus_messages_la_SOURCES = \
libpraefectus/messages/PraefAdvSerNo.c \
libpraefectus/messages/PraefByte.c \
libpraefectus/messages/PraefDword.c \
libpraefectus/messages/PraefHash.c \
libpraefectus/messages/PraefHashPrefix.c \
libpraefectus/messages/PraefHtdirEntry.c \
libpraefectus/messages/PraefInstant.c \
libpraefectus/messages/PraefIpAddress.c \
libpraefectus/messages/PraefMsg.c \
libpraefectus/messages/PraefMsgAck.c \
libpraefectus/messages/PraefMsgAppEvent.c \
libpraefectus/messages/PraefMsgAppUnicast.c \
libpraefectus/messages/PraefMsgChmod.c \
libpraefectus/messages/PraefMsgCommit.c \
libpraefectus/messages/PraefMsgGetNetworkInfo.c \
libpraefectus/messages/PraefMsgHtDir.c \
libpraefectus/messages/PraefMsgHtLs.c \
libpraefectus/messages/PraefMsgHtRange.c \
libpraefectus/messages/PraefMsgHtRangeNext.c \
libpraefectus/messages/PraefMsgHtRangeUnknown.c \
libpraefectus/messages/PraefMsgHtRead.c \
libpraefectus/messages/PraefMsgJoinAccept.c \
libpraefectus/messages/PraefMsgJoinRequest.c \
libpraefectus/messages/PraefMsgJoinTree.c \
libpraefectus/messages/PraefMsgJoinTreeEntry.c \
libpraefectus/messages/PraefMsgNetworkInfo.c \
libpraefectus/messages/PraefMsgPing.c \
libpraefectus/messages/PraefMsgPong.c \
libpraefectus/messages/PraefMsgReceived.c \
libpraefectus/messages/PraefMsgRoute.c \
libpraefectus/messages/PraefMsgVote.c \
libpraefectus/messages/PraefMsgWhoIs.c \
libpraefectus/messages/PraefNetworkIdentifier.c \
libpraefectus/messages/PraefNetworkIdentifierPair.c \
libpraefectus/messages/PraefNetworkSalt.c \
libpraefectus/messages/PraefNodeId.c \
libpraefectus/messages/PraefPingId.c \
libpraefectus/messages/PraefPubkey.c \
libpraefectus/messages/PraefSerialNumber.c \
libpraefectus/messages/PraefShort.c \
libpraefectus/messages/PraefSignature.c

libasn1_support_la_CFLAGS = -w -Iasn1-support
libasn1_support_la_SOURCES = \
asn1-support/BIT_STRING.c \
asn1-support/BOOLEAN.c \
asn1-support/INTEGER.c \
asn1-support/NULL.c \
asn1-support/NativeEnumerated.c \
asn1-support/NativeInteger.c \
asn1-support/OCTET_STRING.c \
asn1-support/PrintableString.c \
asn1-support/VisibleString.c \
asn1-support/asn_SEQUENCE_OF.c \
asn1-support/asn_SET_OF.c \
asn1-support/asn_codecs_prim.c \
asn1-support/ber_decoder.c \
asn1-support/ber_tlv_length.c \
asn1-support/ber_tlv_tag.c \
asn1-support/constr_CHOICE.c \
asn1-support/constr_SEQUENCE.c \
asn1-support/constr_SEQUENCE_OF.c \
asn1-support/constr_SET_OF.c \
asn1-support/constr_TYPE.c \
asn1-support/constraints.c \
asn1-support/der_encoder.c \
asn1-support/per_decoder.c \
asn1-support/per_encoder.c \
asn1-support/per_opentype.c \
asn1-support/per_support.c \
asn1-support/xer_decoder.c \
asn1-support/xer_encoder.c \
asn1-support/xer_support.c

libudp_common_la_CFLAGS = -w -Iasn1-support -Ilibpraefectus/messages
libudp_common_la_SOURCES = \
udp-common/PraefUdpMsg.c \
udp-common/PraefUdpMsgAdvertise.c \
udp-common/PraefUdpMsgDiscover.c \
udp-common/PraefUdpMsgEcho.c \
udp-common/PraefUdpMsgRegister.c \
udp-common/PraefUdpMsgWhoAmI.c \
udp-common/PraefUdpMsgYouAre.c

nbodies_libnbodies_core_la_SOURCES = nbodies/nbodies-core.c
nbodies_nbodies_vnet_4_ideal_SOURCES = nbodies/nbodies-vnet-4-ideal.c
nbodies_nbodies_vnet_4_ideal_LDADD = \
  nbodies/libnbodies-core.la libpraefectus.la
nbodies_nbodies_vnet_8_latent_lossy_SOURCES = \
  nbodies/nbodies-vnet-8-latent-lossy.c
nbodies_nbodies_vnet_8_latent_lossy_LDADD = \
  nbodies/libnbodies-core.la libpraefectus.la

nbodies_nbodies_vis_SOURCES = nbodies/nbodies-vis.c
