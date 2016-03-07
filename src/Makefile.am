
AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

# Recursing into asn1-support first has the nice effect of forcing everything
# there to be built first, so we don't need to go through the headache of
# getting Automake to comprehend that a task produces multiple header files.
SUBDIRS = libpraefectus udp-common vertexd/config \
	praefectus/asn1 .

AM_CFLAGS=-Ilibpraefectus/asn1-support -D_BSD_SOURCE $(SDL_CFLAGS)

if BUILD_DEMOS
demo_bin = praef
demo_noinst = nbodies/nbodies-vnet-4-ideal nbodies/nbodies-vis \
  nbodies/nbodies-vnet-8-latent-lossy chat-demo/chat-demo
demo_libs = nbodies/libnbodies-core.la libpraef-asn1.la
else
demo_bin =
demo_noinst =
demo_libs =
endif

if BUILD_VERTEXD
vertexd_bin = praef-vertexd
vertexd_libs = libvertexd-config.la
else
vertexd_bin =
vertexd_libs =
endif

bin_PROGRAMS = $(demo_bin) $(vertexd_bin)
lib_LTLIBRARIES = libpraefectus.la libpraefectus-umb.la
noinst_PROGRAMS = $(demo_noinst)
noinst_LTLIBRARIES = libasn1-support.la \
libpraefectus-messages.la $(demo_libs) \
libudp-common.la $(vertexd_libs)

praef_LDADD = libpraefectus.la libpraefectus-umb.la \
              libpraef-asn1.la $(SDL_LIBS)
praef_SOURCES = \
praefectus/global-config.c \
praefectus/top/main.c \
praefectus/top/main-menu.c \
praefectus/graphics/canvas.c \
praefectus/graphics/crt.c \
praefectus/graphics/font.c \
praefectus/graphics/console.c \
praefectus/ui/menu.c \
praefectus/game/context.c \
praefectus/game/object.c \
praefectus/game/event.c \
praefectus/game/gameplay.c

praef_vertexd_CFLAGS = $(AM_CFLAGS) -Ilibpraefectus/messages
praef_vertexd_LDADD = libudp-common.la libvertexd-config.la
praef_vertexd_SOURCES = vertexd/vertexd.c

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
libpraefectus/stdsys.c \
libpraefectus/simple.c \
libpraefectus/flat-netid.c \
libpraefectus/system-stderr-log.c

libpraefectus_la_LIBADD = libpraefectus-messages.la

libpraefectus_umb_la_CFLAGS = $(AM_CFLAGS) -Ilibpraefectus/messages -Iudp-common

libpraefectus_umb_la_SOURCES = \
  libpraefectus/udp-message-bus/udp-message-bus.c

libpraefectus_umb_la_LIBADD = \
  libpraefectus.la libudp-common.la

libpraefectus_messages_la_CFLAGS = -w -Ilibpraefectus/asn1-support
libpraefectus_messages_la_LIBADD = libasn1-support.la
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

libasn1_support_la_CFLAGS = -w -Ilibpraefectus/asn1-support
libasn1_support_la_SOURCES = \
libpraefectus/asn1-support/BIT_STRING.c \
libpraefectus/asn1-support/BOOLEAN.c \
libpraefectus/asn1-support/INTEGER.c \
libpraefectus/asn1-support/NULL.c \
libpraefectus/asn1-support/NativeEnumerated.c \
libpraefectus/asn1-support/NativeInteger.c \
libpraefectus/asn1-support/OCTET_STRING.c \
libpraefectus/asn1-support/PrintableString.c \
libpraefectus/asn1-support/VisibleString.c \
libpraefectus/asn1-support/UTF8String.c \
libpraefectus/asn1-support/asn_SEQUENCE_OF.c \
libpraefectus/asn1-support/asn_SET_OF.c \
libpraefectus/asn1-support/asn_codecs_prim.c \
libpraefectus/asn1-support/ber_decoder.c \
libpraefectus/asn1-support/ber_tlv_length.c \
libpraefectus/asn1-support/ber_tlv_tag.c \
libpraefectus/asn1-support/constr_CHOICE.c \
libpraefectus/asn1-support/constr_SEQUENCE.c \
libpraefectus/asn1-support/constr_SEQUENCE_OF.c \
libpraefectus/asn1-support/constr_SET.c \
libpraefectus/asn1-support/constr_SET_OF.c \
libpraefectus/asn1-support/constr_TYPE.c \
libpraefectus/asn1-support/constraints.c \
libpraefectus/asn1-support/der_encoder.c \
libpraefectus/asn1-support/per_decoder.c \
libpraefectus/asn1-support/per_encoder.c \
libpraefectus/asn1-support/per_opentype.c \
libpraefectus/asn1-support/per_support.c \
libpraefectus/asn1-support/xer_decoder.c \
libpraefectus/asn1-support/xer_encoder.c \
libpraefectus/asn1-support/xer_support.c

libudp_common_la_CFLAGS = -w -Ilibpraefectus/asn1-support -Ilibpraefectus/messages
libudp_common_la_LIBADD = libpraefectus-messages.la
libudp_common_la_SOURCES = \
udp-common/PraefUdpMsg.c \
udp-common/PraefUdpMsgAdvertise.c \
udp-common/PraefUdpMsgDiscover.c \
udp-common/PraefUdpMsgEcho.c \
udp-common/PraefUdpMsgRegister.c \
udp-common/PraefUdpMsgWhoAmI.c \
udp-common/PraefUdpMsgYouAre.c

libvertexd_config_la_CFLAGS = -w -Ilibpraefectus/asn1-support
libvertexd_config_la_LIBADD = libasn1-support.la
libvertexd_config_la_SOURCES = \
vertexd/config/VertexdConfiguration.c \
vertexd/config/Realm.c

libpraef_asn1_la_CFLAGS = -w -Ilibpraefectus/asn1-support
libpraef_asn1_la_LIBADD = libasn1-support.la
libpraef_asn1_la_SOURCES = \
praefectus/asn1/BoundKey.c \
praefectus/asn1/ControlsConfiguration.c \
praefectus/asn1/GameEvent.c \
praefectus/asn1/GameEventInitialise.c \
praefectus/asn1/GameEventSetControl.c \
praefectus/asn1/GameEventFire.c \
praefectus/asn1/PraefectusConfiguration.c

nbodies_libnbodies_core_la_SOURCES = nbodies/nbodies-core.c
nbodies_nbodies_vnet_4_ideal_SOURCES = nbodies/nbodies-vnet-4-ideal.c
nbodies_nbodies_vnet_4_ideal_LDADD = \
  nbodies/libnbodies-core.la libpraefectus.la
nbodies_nbodies_vnet_8_latent_lossy_SOURCES = \
  nbodies/nbodies-vnet-8-latent-lossy.c
nbodies_nbodies_vnet_8_latent_lossy_LDADD = \
  nbodies/libnbodies-core.la libpraefectus.la

nbodies_nbodies_vis_SOURCES = nbodies/nbodies-vis.c
nbodies_nbodies_vis_LDADD = $(SDL_LIBS)

chat_demo_chat_demo_SOURCES = chat-demo/chat-demo.c
chat_demo_chat_demo_LDADD = libpraefectus.la libpraefectus-umb.la $(SDL_LIBS)

include_HEADERS = \
libpraefectus/ack-table.h \
libpraefectus/asn1-support/BIT_STRING.h \
libpraefectus/asn1-support/BOOLEAN.h \
libpraefectus/asn1-support/INTEGER.h \
libpraefectus/asn1-support/NULL.h \
libpraefectus/asn1-support/NativeEnumerated.h \
libpraefectus/asn1-support/NativeInteger.h \
libpraefectus/asn1-support/OCTET_STRING.h \
libpraefectus/asn1-support/PraefDummy.h \
libpraefectus/asn1-support/PraefDummyChoice.h \
libpraefectus/asn1-support/PraefDummySet.h \
libpraefectus/asn1-support/PrintableString.h \
libpraefectus/asn1-support/UTF8String.h \
libpraefectus/asn1-support/VisibleString.h \
libpraefectus/asn1-support/asn_SEQUENCE_OF.h \
libpraefectus/asn1-support/asn_SET_OF.h \
libpraefectus/asn1-support/asn_application.h \
libpraefectus/asn1-support/asn_codecs.h \
libpraefectus/asn1-support/asn_codecs_prim.h \
libpraefectus/asn1-support/asn_internal.h \
libpraefectus/asn1-support/asn_system.h \
libpraefectus/asn1-support/ber_decoder.h \
libpraefectus/asn1-support/ber_tlv_length.h \
libpraefectus/asn1-support/ber_tlv_tag.h \
libpraefectus/asn1-support/constr_CHOICE.h \
libpraefectus/asn1-support/constr_SEQUENCE.h \
libpraefectus/asn1-support/constr_SEQUENCE_OF.h \
libpraefectus/asn1-support/constr_SET.h \
libpraefectus/asn1-support/constr_SET_OF.h \
libpraefectus/asn1-support/constr_TYPE.h \
libpraefectus/asn1-support/constraints.h \
libpraefectus/asn1-support/der_encoder.h \
libpraefectus/asn1-support/per_decoder.h \
libpraefectus/asn1-support/per_encoder.h \
libpraefectus/asn1-support/per_opentype.h \
libpraefectus/asn1-support/per_support.h \
libpraefectus/asn1-support/xer_decoder.h \
libpraefectus/asn1-support/xer_encoder.h \
libpraefectus/asn1-support/xer_support.h \
libpraefectus/bsd.h \
libpraefectus/clock.h \
libpraefectus/commitment-chain.h \
libpraefectus/common.h \
libpraefectus/context.h \
libpraefectus/defs.h \
libpraefectus/dsa-parms.h \
libpraefectus/dsa.h \
libpraefectus/event.h \
libpraefectus/flat-netid.h \
libpraefectus/hash-tree.h \
libpraefectus/hl-msg.h \
libpraefectus/message-bus.h \
libpraefectus/messages/PraefAdvSerNo.h \
libpraefectus/messages/PraefByte.h \
libpraefectus/messages/PraefDword.h \
libpraefectus/messages/PraefHash.h \
libpraefectus/messages/PraefHashPrefix.h \
libpraefectus/messages/PraefHtdirEntry.h \
libpraefectus/messages/PraefInstant.h \
libpraefectus/messages/PraefIpAddress.h \
libpraefectus/messages/PraefMsg.h \
libpraefectus/messages/PraefMsgAck.h \
libpraefectus/messages/PraefMsgAppEvent.h \
libpraefectus/messages/PraefMsgAppUnicast.h \
libpraefectus/messages/PraefMsgChmod.h \
libpraefectus/messages/PraefMsgCommit.h \
libpraefectus/messages/PraefMsgGetNetworkInfo.h \
libpraefectus/messages/PraefMsgHtDir.h \
libpraefectus/messages/PraefMsgHtLs.h \
libpraefectus/messages/PraefMsgHtRange.h \
libpraefectus/messages/PraefMsgHtRangeNext.h \
libpraefectus/messages/PraefMsgHtRangeUnknown.h \
libpraefectus/messages/PraefMsgHtRead.h \
libpraefectus/messages/PraefMsgJoinAccept.h \
libpraefectus/messages/PraefMsgJoinRequest.h \
libpraefectus/messages/PraefMsgJoinTree.h \
libpraefectus/messages/PraefMsgJoinTreeEntry.h \
libpraefectus/messages/PraefMsgNetworkInfo.h \
libpraefectus/messages/PraefMsgPing.h \
libpraefectus/messages/PraefMsgPong.h \
libpraefectus/messages/PraefMsgReceived.h \
libpraefectus/messages/PraefMsgRoute.h \
libpraefectus/messages/PraefMsgVote.h \
libpraefectus/messages/PraefMsgWhoIs.h \
libpraefectus/messages/PraefNetworkIdentifier.h \
libpraefectus/messages/PraefNetworkIdentifierPair.h \
libpraefectus/messages/PraefNetworkSalt.h \
libpraefectus/messages/PraefNodeId.h \
libpraefectus/messages/PraefPingId.h \
libpraefectus/messages/PraefPubkey.h \
libpraefectus/messages/PraefSerialNumber.h \
libpraefectus/messages/PraefShort.h \
libpraefectus/messages/PraefSignature.h \
libpraefectus/metatransactor.h \
libpraefectus/mtt-bridge.h \
libpraefectus/object.h \
libpraefectus/outbox.h \
libpraefectus/secure-random.h \
libpraefectus/simple.h \
libpraefectus/std-state.h \
libpraefectus/stdsys.h \
libpraefectus/system-stderr-log.h \
libpraefectus/system.h \
libpraefectus/transactor.h \
libpraefectus/udp-message-bus/udp-message-bus.h \
libpraefectus/virtual-bus.h
