lib_LTLIBRARIES = libdgmail.la
libdgmail_la_SOURCES = libdgmail.c
libdgmail_la_CFLAGS = -I../libdgstr
DEFS += -DHAVE_STRNDUP
AM_FLAGS = -DHAVE_STRNDUP
LINK = $(LIBTOOL) --tag=CC --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
        $(AM_LDFLAGS) -o $@
