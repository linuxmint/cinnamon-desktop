cdesktopincludedir = $(includedir)/cinnamon-desktop
cdesktopinclude_HEADERS = cdesktop-enums.h

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

CLEANFILES =

if HAVE_INTROSPECTION

CDesktopEnums_3_0_gir_CFLAGS = -I$(srcdir)
CDesktopEnums_3_0_gir_FILES = cdesktop-enums.h
CDesktopEnums_3_0_gir_SCANNERFLAGS = --header-only --identifier-prefix=CDesktop
INTROSPECTION_GIRS += CDesktopEnums-3.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

-include $(top_srcdir)/git.mk
-include $(INTROSPECTION_MAKEFILE)
