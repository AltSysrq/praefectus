AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=
SUBDIRS = asn1-support messages .

noinst_HEADERS = praef-config.h

praef-config.h: $(top_srcdir)/config.h
	$(AM_V_GEN)$(SED) <$(top_srcdir)/config.h >praef-config.h \
		'/[Rr]estrict/!s/# *define /#define PRAEF_/;/[Rr]estrict/!s/# *undef /#undef PRAEF_/'
