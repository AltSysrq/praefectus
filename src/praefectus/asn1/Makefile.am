AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

noinst_DATA = PraefectusConfiguration.c GameEvent.c

PraefectusConfiguration.c: configuration.asn1
	$(ASN1C) -fcompound-names -fnative-types -R \
		configuration.asn1
	touch PraefectusConfiguration.c

GameEvent.c: events.asn1
	$(ASN1C) -fcompound-names -fnative-types -gen-PER -R \
		events.asn1
	touch GameEvent.c

clean: clean-am
	rm -f *.h *.c
