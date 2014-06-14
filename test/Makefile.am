AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

TESTS = libpraefectus/core
AM_CFLAGS = $(CHECK_CFLAGS) -I$(top_builddir)/src -Wall -Wextra \
  -Wno-unused-parameter -Wno-long-long -Wno-missing-field-initializers \
  -std=gnu99 -Wno-gnu

TEST_LIBS = $(CHECK_LIBS) libtestcore.la

check_PROGRAMS = $(TESTS)
noinst_LTLIBRARIES = libtestcore.la
libtestcore_la_SOURCES = test.c
libpraefectus_core_SOURCES = libpraefectus/core.c
libpraefectus_core_LDADD = $(TEST_LIBS) ../src/libpraefectus.la
