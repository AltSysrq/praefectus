AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

noinst_DATA = PraefMsg.c

PraefMsg.c: messages.asn1
	$(ASN1C) -fcompound-names -gen-PER -R messages.asn1
	touch PraefMsg.c

clean: clean-am
	rm -f *.h *.c
