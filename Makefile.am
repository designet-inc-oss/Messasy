bin_PROGRAMS = messasy
#bin_SCRIPTS = ../bin/messasyctl
messasy_SOURCES = client_side.c filter.c messasy.c msy_config.c utils.c msy_readmodule.c
messasy_LDADD = -lmilter -lpthread -lldap -ldgstr -ldgconfig -ldgmail -ldl @LIB_WRAP@
AM_LDFLAGS = @LDFLAGS@
SO_LDFLAGS = @SO_LDFLAGS@
#sysconf_DATA = messasy.conf
AM_CFLAGS = -g -Wall -fPIC
SO_CFLAGS=@SO_CFLAGS@
SO_AM_CFLAGS = -g -Wall -fPIC
DEFS = @DEFS@ -DHAVE_STRNDUP -DDEFAULT_CONFDIR=\"$(sysconfdir)\" @DEF_LIBWRAP@
#SUBDIRS = lib . so
SUBDIRS = lib . so
EXEEXT = .bin
