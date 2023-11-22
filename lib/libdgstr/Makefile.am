lib_LTLIBRARIES = libdgstr.la
libdgstr_la_SOURCES = libdgstr.c
DEFS += -DHAVE_STRNDUP
AM_CFLAGS = -DHAVE_STRNDUP
LINK = $(LIBTOOL) --tag=CC --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
        $(AM_LDFLAGS) -o $@
