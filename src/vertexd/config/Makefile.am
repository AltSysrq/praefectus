AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

noinst_DATA = VertexdConfiguration.c

VertexdConfiguration.c: config.asn1
	$(ASN1C) -fcompound-names -fnative-types -gen-PER -R \
		config.asn1
	touch VertexdConfiguration.c

clean: clean-am
	rm -f *.h *.c
