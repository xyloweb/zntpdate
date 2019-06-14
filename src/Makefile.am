# Makefile.am ./src
bin_PROGRAMS=zntpdate

noinst_HEADERS = trace.h ntpdate.h main.h gettext.h

zntpdate_SOURCES=main.c ntpdate.c trace.c

datadir = @datadir@
localedir = $(datadir)/locale
DEFS = -DPACKAGE_LOCAL_DIR=\"$(localedir)\" @DEFS@