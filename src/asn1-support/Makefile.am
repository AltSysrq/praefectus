AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

noinst_DATA = per_decoder.c

per_decoder.c: dummy.asn1
	rm -f *.h *.c
	$(ASN1C) -fcompound-names -gen-PER dummy.asn1 >/dev/null 2>&1

clean: clean-am
	rm -f *.h *.c
