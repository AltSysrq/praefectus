AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

noinst_DATA = PraefectusConfiguration.c

PraefectusConfiguration.c: configuration.asn1
	$(ASN1C) -fcompound-names -fnative-types -R \
		configuration.asn1
	touch PraefectusConfiguration.c

clean: clean-am
	rm -f *.h *.c
