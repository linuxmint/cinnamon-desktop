CLEANFILES =

noinst_LTLIBRARIES =
lib_LTLIBRARIES = libcinnamon-desktop.la

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	-I$(srcdir)/libgsystem					\
	$(CINNAMON_DESKTOP_CFLAGS)					\
	$(XLIB_CFLAGS)						\
	-DG_LOG_DOMAIN=\"CinnamonDesktop\"				\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale\""	\
	-DPNP_IDS=\""$(PNP_IDS)"\"				\
	-DXKB_BASE=\""$(XKB_BASE)"\"				\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

libgsystem_srcpath := libgsystem
libgsystem_cflags = $(CINNAMON_DESKTOP_CFLAGS)
libgsystem_libs = $(CINNAMON_DESKTOP_LIBS)
include libgsystem/Makefile-libgsystem.am

introspection_sources = 		\
	gnome-desktop-utils.c		\
	gnome-desktop-thumbnail.c 	\
	gnome-thumbnail-pixbuf-utils.c 	\
	gnome-bg.c			\
	gnome-bg-crossfade.c		\
	display-name.c			\
    gnome-installer.c       \
	gnome-rr.c			\
	gnome-rr-config.c		\
	gnome-rr-output-info.c		\
	gnome-rr-labeler.c		\
	gnome-pnp-ids.c			\
	gnome-wall-clock.c		\
	gnome-xkb-info.c		\
	edid-parse.c

libcinnamon_desktop_la_SOURCES = 	\
	$(introspection_sources)	\
	gnome-datetime-source.h		\
	gnome-datetime-source.c		\
	gnome-rr-private.h		\
	edid.h				\
	private.h

libcinnamon_desktop_la_LIBADD =	\
	$(XLIB_LIBS)		\
	$(LIBM)			\
	$(CINNAMON_DESKTOP_LIBS)	\
	libgsystem.la           \
	-lrt

libcinnamon_desktop_la_LDFLAGS = \
	-version-info $(LT_VERSION) \
	-export-symbols-regex "^gnome_.*" \
	-no-undefined

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cinnamon-desktop.pc

libcinnamon_desktopdir = $(includedir)/cinnamon-desktop/libcinnamon-desktop
libcinnamon_desktop_HEADERS = \
        gnome-bg.h                      \
        gnome-bg-crossfade.h            \
        gnome-desktop-utils.h           \
        gnome-desktop-thumbnail.h       \
        gnome-installer.h               \
        gnome-rr.h                      \
        gnome-rr-config.h               \
        gnome-rr-labeler.h		\
        gnome-pnp-ids.h			\
        gnome-wall-clock.h		\
	gnome-xkb-info.h		\
	cdesktop-enums.h

if USE_INTERNAL_PNP_IDS
pnpdatadir = $(datadir)/libcinnamon-desktop
pnpdata_DATA = pnp.ids
endif
pnpdata_DATA_dist = pnp.ids

check:
	test -s $(top_srcdir)/libcinnamon-desktop/pnp.ids

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION


CDesktopEnums_3_0_gir_CFLAGS = -I$(srcdir)
CDesktopEnums_3_0_gir_FILES = cdesktop-enums.h
CDesktopEnums_3_0_gir_SCANNERFLAGS = --header-only --identifier-prefix=CDesktop
INTROSPECTION_GIRS += CDesktopEnums-3.0.gir
CinnamonDesktop-3.0.gir: libcinnamon-desktop.la
CinnamonDesktop_3_0_gir_INCLUDES = GObject-2.0 Gtk-3.0
CinnamonDesktop_3_0_gir_PACKAGES = gdk-pixbuf-2.0 glib-2.0 gobject-2.0 gio-2.0 gtk+-3.0
CinnamonDesktop_3_0_gir_EXPORT_PACKAGES = cinnamon-desktop
CinnamonDesktop_3_0_gir_CFLAGS = -DGNOME_DESKTOP_USE_UNSTABLE_API -I$(top_srcdir)
CinnamonDesktop_3_0_gir_LIBS = libcinnamon-desktop.la
CinnamonDesktop_3_0_gir_FILES = $(introspection_sources) $(libcinnamon_desktop_HEADERS)
CinnamonDesktop_3_0_gir_SCANNERFLAGS = --identifier-prefix=Gnome --symbol-prefix=gnome_
INTROSPECTION_SCANNER_ARGS += $(patsubst %,--c-include='libcinnamon-desktop/%',$(libcinnamon_desktop_HEADERS))
INTROSPECTION_GIRS += CinnamonDesktop-3.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

EXTRA_DIST = \
	cinnamon-desktop.pc.in			\
	cinnamon-desktop-uninstalled.pc.in	\
	$(pnpdata_DATA_dist)

MAINTAINERCLEANFILES = \
	pnp.ids

libexec_PROGRAMS =		\
	cinnamon-rr-debug

cinnamon_rr_debug_SOURCES =	\
	cinnamon-rr-debug.c

cinnamon_rr_debug_LDADD =		\
	$(CINNAMON_DESKTOP_LIBS)	\
	$(lib_LTLIBRARIES)	\
	$(XLIB_LIBS)

noinst_PROGRAMS = test-xkb-info test-pnp-ids test-wall-clock
test_xkb_info_LDADD =		\
	$(CINNAMON_DESKTOP_LIBS)	\
	$(lib_LTLIBRARIES)	\
	$(XLIB_LIBS)

test_pnp_ids_LDADD = $(test_xkb_info_LDADD)

test_wall_clock_LDADD = $(test_xkb_info_LDADD)

-include $(top_srcdir)/git.mk
