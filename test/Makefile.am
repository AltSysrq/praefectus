AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

TESTS = libpraefectus/core libpraefectus/core-fullstack \
libpraefectus/transactor libpraefectus/metatransactor \
libpraefectus/hash-tree
AM_CFLAGS = $(CHECK_CFLAGS) -I$(top_builddir)/src -Wall -Wextra \
  -Wno-unused-parameter -Wno-long-long -Wno-missing-field-initializers \
  -std=gnu99 -Wno-gnu

TEST_LIBS = libtestcore.la $(CHECK_LIBS) ../src/libpraefectus.la

check_PROGRAMS = $(TESTS)
noinst_LTLIBRARIES = libtestcore.la
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
