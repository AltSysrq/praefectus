AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

# Recursing into asn1-support first has the nice effect of forcing everything
# there to be built first, so we don't need to go through the headache of
# getting Automake to comprehend that a task produces multiple header files.
SUBDIRS = asn1-support .

AM_CFLAGS="-I$(top_srcdir)/asn1-support"

noinst_LTLIBRARIES = libpraefectus.la
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
libpraefectus/commitment-chain.c
