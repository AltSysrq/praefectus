AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

noinst_DATA = dummy.c

dummy.c: dummy.asn1
	rm -f *.h *.c
	$(ASN1C) -fcompound-names -gen-PER dummy.asn1 >/dev/null 2>&1
	touch dummy.c

clean: clean-am
	rm -f *.h *.c
