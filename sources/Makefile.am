libdir           = @plugindir@
lib_LTLIBRARIES  = omegle.la

omegle_la_CFLAGS  = $(AM_CFLAGS) $(BITLBEE_CFLAGS) $(GLIB_CFLAGS) $(JANSSON_CFLAGS)
omegle_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS) $(JANSSON_LIBS)
omegle_la_SOURCES = omegle.c
