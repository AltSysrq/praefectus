AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

TESTS = libpraefectus/core libpraefectus/core-fullstack \
libpraefectus/transactor libpraefectus/metatransactor \
libpraefectus/hash-tree libpraefectus/commitment-chain \
libpraefectus/dsa libpraefectus/virtual-bus
AM_CFLAGS = $(CHECK_CFLAGS) -I$(top_builddir)/src \
  -I$(top_builddir)/src/asn1-support -Wall -Wextra \
  -Wno-unused-parameter -Wno-long-long -Wno-missing-field-initializers \
  -std=gnu99 -Wno-gnu

TEST_LIBS = libtestcore.la $(CHECK_LIBS) ../src/libpraefectus.la

check_PROGRAMS = $(TESTS)
check_LTLIBRARIES = libtestcore.la
libtestcore_la_SOURCES = test.c
libpraefectus_core_SOURCES = libpraefectus/core.c
libpraefectus_core_LDADD = $(TEST_LIBS)
libpraefectus_core_fullstack_SOURCES = libpraefectus/core-fullstack.c
libpraefectus_core_fullstack_LDADD = $(TEST_LIBS)
libpraefectus_transactor_SOURCES = libpraefectus/transactor.c
libpraefectus_transactor_LDADD = $(TEST_LIBS)
libpraefectus_metatransactor_SOURCES = libpraefectus/metatransactor.c
libpraefectus_metatransactor_LDADD = $(TEST_LIBS)
libpraefectus_hash_tree_SOURCES = libpraefectus/hash-tree.c
libpraefectus_hash_tree_LDADD = $(TEST_LIBS)
libpraefectus_commitment_chain_SOURCES = libpraefectus/commitment-chain.c
libpraefectus_commitment_chain_LDADD = $(TEST_LIBS)
libpraefectus_dsa_SOURCES = libpraefectus/dsa.c
libpraefectus_dsa_LDADD = $(TEST_LIBS)
libpraefectus_virtual_bus_SOURCES = libpraefectus/virtual-bus.c
libpraefectus_virtual_bus_LDADD = $(TEST_LIBS)
