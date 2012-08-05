libdir           = @plugindir@
lib_LTLIBRARIES  = omegle.la

omegle_la_CFLAGS  = $(BITLBEE_CFLAGS) $(GLIB_CFLAGS) -Wall
omegle_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)
omegle_la_SOURCES = omegle.c
