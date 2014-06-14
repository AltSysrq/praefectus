# NetBSD make (now also used on FreeBSD) for some reason prints
#   --- TARGET ---
# before each target that is built when parallel make is used, even though that
# information is redundant with the command printed for the target anyway (ie,
# CC TARGET in silent mode, the whole cc line otherwise). Suppress this.
.MAKE.JOB.PREFIX=

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src test
