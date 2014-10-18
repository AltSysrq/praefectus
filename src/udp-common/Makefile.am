AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

noinst_DATA = PraefUdpMsg.c

PraefUdpMsg.c: udp-messages.asn1
	$(ASN1C) -fcompound-names -fnative-types -gen-PER -R \
		-fknown-extern-type=PraefDword \
		-fknown-extern-type=PraefNetworkIdentifier \
		-fknown-extern-type=PraefNetworkIdentifierPair \
		udp-messages.asn1
	touch PraefUdpMsg.c

clean: clean-am
	rm -f *.h *.c
