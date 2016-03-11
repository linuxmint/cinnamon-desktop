lib_LTLIBRARIES = libcvc.la

libcvc_la_CPPFLAGS =			\
	$(WARN_CFLAGS)			\
	$(CVC_CFLAGS)			\
	-I$(srcdir)			\
	-DWITH_INTROSPECTION		\
	-DG_LOG_DOMAIN="\"Cvc\""

libcvc_la_gir_sources = 			\
	gvc-mixer-card.c			\
	gvc-mixer-stream.c			\
	gvc-channel-map.c			\
	gvc-mixer-ui-device.c			\
	gvc-mixer-sink.c			\
    gvc-mixer-sink.h            \
	gvc-mixer-source.c			\
	gvc-mixer-sink-input.c			\
	gvc-mixer-source-output.c		\
    gvc-mixer-event-role.h          \
	gvc-mixer-event-role.c			\
	gvc-mixer-control.c			\
	$(NULL)

libcvc_la_SOURCES =				\
	$(libcvc_la_gir_sources)		\
	gvc-mixer-card-private.h		\
	gvc-mixer-stream-private.h		\
	gvc-channel-map-private.h		\
	gvc-mixer-control-private.h		\
	gvc-pulseaudio-fake.h			\
	$(NULL)

libcvc_la_LIBADD = 	\
	$(CVC_LIBS)	\
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cvc.pc

libcvcdir = $(includedir)/cinnamon-desktop/libcvc
libcvc_HEADERS = \
    gvc-mixer-card.h            \
    gvc-mixer-stream.h          \
    gvc-channel-map.h           \
    gvc-mixer-ui-device.h           \
    gvc-mixer-source.h          \
    gvc-mixer-sink-input.h          \
    gvc-mixer-source-output.h       \
    gvc-mixer-control.h

if HAVE_INTROSPECTION

include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

Cvc-1.0.gir: libcvc.la
Cvc_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
Cvc_1_0_gir_CFLAGS = $(INCLUDES) -I$(top_srcdir)/libcvc -DWITH_INTROSPECTION
Cvc_1_0_gir_LIBS = libcvc.la
Cvc_1_0_gir_SCANNERFLAGS = --identifier-prefix=Gvc --symbol-prefix=gvc_
Cvc_1_0_gir_FILES = $(libcvc_la_gir_sources) $(libcvc_HEADERS)
INTROSPECTION_GIRS = Cvc-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = Cvc-1.0.gir

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)

endif

EXTRA_DIST = \
    cvc.pc.in          \
    cvc-uninstalled.pc.in

