AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

TESTS = libpraefectus/core.t libpraefectus/core-fullstack.t \
libpraefectus/transactor.t libpraefectus/metatransactor.t \
libpraefectus/hash-tree.t libpraefectus/commitment-chain.t \
libpraefectus/dsa.t libpraefectus/virtual-bus.t \
libpraefectus/hl-msg.t libpraefectus/clock.t \
libpraefectus/outbox.t libpraefectus/ack-table.t \
libpraefectus/system.t libpraefectus/system-unit.t
AM_CFLAGS = $(CHECK_CFLAGS) -I$(top_builddir)/src \
  -I$(top_builddir)/src/asn1-support -Wall -Wextra \
  -Wno-unused-parameter -Wno-long-long -Wno-missing-field-initializers \
  -std=gnu99 -Wno-gnu

TEST_LIBS = libtestcore.la $(CHECK_LIBS) ../src/libpraefectus.la

noinst_PROGRAMS = libpraefectus/clock-sim
check_PROGRAMS = $(TESTS)
check_LTLIBRARIES = libtestcore.la
libtestcore_la_SOURCES = test.c
libpraefectus_core_t_SOURCES = libpraefectus/core.c
libpraefectus_core_t_LDADD = $(TEST_LIBS)
libpraefectus_core_fullstack_t_SOURCES = libpraefectus/core-fullstack.c
libpraefectus_core_fullstack_t_LDADD = $(TEST_LIBS)
libpraefectus_transactor_t_SOURCES = libpraefectus/transactor.c
libpraefectus_transactor_t_LDADD = $(TEST_LIBS)
libpraefectus_metatransactor_t_SOURCES = libpraefectus/metatransactor.c
libpraefectus_metatransactor_t_LDADD = $(TEST_LIBS)
libpraefectus_hash_tree_t_SOURCES = libpraefectus/hash-tree.c
libpraefectus_hash_tree_t_LDADD = $(TEST_LIBS)
libpraefectus_commitment_chain_t_SOURCES = libpraefectus/commitment-chain.c
libpraefectus_commitment_chain_t_LDADD = $(TEST_LIBS)
libpraefectus_dsa_t_SOURCES = libpraefectus/dsa.c
libpraefectus_dsa_t_LDADD = $(TEST_LIBS)
libpraefectus_virtual_bus_t_SOURCES = libpraefectus/virtual-bus.c
libpraefectus_virtual_bus_t_LDADD = $(TEST_LIBS)
libpraefectus_hl_msg_t_SOURCES = libpraefectus/hl-msg.c
libpraefectus_hl_msg_t_LDADD = $(TEST_LIBS)
libpraefectus_clock_t_SOURCES = libpraefectus/clock.c
libpraefectus_clock_t_LDADD = $(TEST_LIBS)
libpraefectus_outbox_t_SOURCES = libpraefectus/outbox.c
libpraefectus_outbox_t_LDADD = $(TEST_LIBS)
libpraefectus_ack_table_t_SOURCES = libpraefectus/ack-table.c
libpraefectus_ack_table_t_LDADD = $(TEST_LIBS)
libpraefectus_system_t_SOURCES = libpraefectus/system.c
libpraefectus_system_t_LDADD = $(TEST_LIBS)
libpraefectus_system_unit_t_SOURCES = libpraefectus/system-unit.c
libpraefectus_system_unit_t_LDADD = $(TEST_LIBS)

libpraefectus_clock_sim_SOURCES = libpraefectus/clock-sim.c
libpraefectus_clock_sim_LDADD = ../src/libpraefectus.la
