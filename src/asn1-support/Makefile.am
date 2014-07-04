AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

noinst_LTLIBRARIES = libasn1-support.la
libasn1_support_la_SOURCES = everything.c
AM_CFLAGS = -w

everything.c: dummy.asn1
	rm -f everything.c
	$(ASN1C) -fcompound-names -gen-PER dummy.asn1 >/dev/null 2>&1
# The DER and PER encoders each define a private symbol of the same name, which
# nominally breaks this concatenation. Since we don't plan on doing DER
# encoding, just drop that source file.
	rm -f der_encoder.c
	cat include-config-h.inc *.c >everything.c

clean: clean-am
	rm -f *.h *.c
