lib_LTLIBRARIES = libgzip.la
libgzip_la_SOURCES = libgzip.c
AM_CFLAGS = -DHAVE_STRNDUP @SO_CFLAGS@ -I../../
AM_LDFLAGS = -shared @SO_LDFLAGS@
libdir = ${exec_prefix}/lib/messasy
LDADD = -L../../ -fPIC ../../utils.o ../../msy_config.o
LINK = $(LIBTOOL) --tag=CC --mode=link $(CCLD) $(AM_CFLAGS) \
        $(AM_LDFLAGS) $(LDADD) -o $@
