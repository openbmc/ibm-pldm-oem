libpldmoemresponder_LTLIBRARIES = libpldmoemresponder.la
libpldmoemresponderdir = ${libdir}
libpldmoemresponder_la_SOURCES = \
	file_io.cpp

libpldmoemresponder_la_LIBADD = \
	../libpldm/libpldmoem.la
libpldmoemresponder_la_LDFLAGS = \
	-version-info 1:0:0 -shared \
        -lstdc++fs
