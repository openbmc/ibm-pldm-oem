AM_CPPFLAGS = -I$(top_srcdir)

TESTS = $(check_PROGRAMS)

check_PROGRAMS = \
	libpldmoem_fileio_test \
	libpldmoemresponder_fileio_test

test_cppflags = \
	-Igtest \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS)

test_cxxflags = \
	$(PTHREAD_CFLAGS)

test_ldflags = \
	-lgtest_main \
	-lgtest \
	$(PTHREAD_LIBS) \
	$(OESDK_TESTCASE_FLAGS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	-lstdc++fs \
	-lgmock

libpldmoem_fileio_test_CPPFLAGS = $(test_cppflags)
libpldmoem_fileio_test_CXXFLAGS = $(test_cxxflags)
libpldmoem_fileio_test_LDFLAGS = $(test_ldflags)
libpldmoem_fileio_test_LDADD = \
	$(top_builddir)/libpldm/base.o \
	$(top_builddir)/libpldm/file_io.o
libpldmoem_fileio_test_SOURCES = libpldm_fileio_test.cpp

libpldmoemresponder_fileio_test_CPPFLAGS = $(test_cppflags)
libpldmoemresponder_fileio_test_CXXFLAGS = $(test_cxxflags)
libpldmoemresponder_fileio_test_LDFLAGS = $(test_ldflags)
libpldmoemresponder_fileio_test_LDADD = \
	$(top_builddir)/libpldm/base.o \
	$(top_builddir)/libpldm/file_io.o \
	$(top_builddir)/libpldmresponder/file_io.o \
	$(top_builddir)/libpldmresponder/file_table.o
libpldmoemresponder_fileio_test_SOURCES = libpldmresponder_fileio_test.cpp

