
AC_INIT(cinnamon-desktop, 1.9.1)
AC_CONFIG_SRCDIR(libcinnamon-desktop)

AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz tar-ustar])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_MACRO_DIR([m4])

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LT_VERSION=4:0:0
AC_SUBST(LT_VERSION)

LT_PREREQ([2.2.6])
LT_INIT([dlopen win32-dll disable-static])

IT_PROG_INTLTOOL([0.40.6])

AC_PROG_CC
PKG_PROG_PKG_CONFIG

GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

AC_ARG_ENABLE(deprecation_flags,
              [AC_HELP_STRING([--enable-deprecation-flags],
                              [use *_DISABLE_DEPRECATED flags @<:@default=no@:>@])],,
              [enable_deprecation_flags=no])

if test "x$enable_deprecation_flags" = "xyes"; then
   DISABLE_DEPRECATED_CFLAGS=$DISABLE_DEPRECATED
   AC_SUBST(DISABLE_DEPRECATED_CFLAGS)
fi

# Path to the pnp.ids file -- to know if we use one shipped with another
# package, or an internal file

AC_ARG_WITH(pnp-ids-path,
              [AC_HELP_STRING([--with-pnp-ids-path],
                              [Specify the path to pnp.ids @<:@default=(internal)@:>@])],,
                              [with_pnp_ids_path="\${pnpdatadir}/pnp.ids"])

AM_CONDITIONAL(USE_INTERNAL_PNP_IDS, test "x$with_pnp_ids_path" = "x\${pnpdatadir}/pnp.ids")
PNP_IDS=$with_pnp_ids_path
AC_SUBST(PNP_IDS)
if test "x$with_pnp_ids_path" = "x\${pnpdatadir}/pnp.ids"; then
        EXTERNAL_PNP_IDS="no (internal)"
else
        EXTERNAL_PNP_IDS="$with_pnp_ids_path"
fi

dnl If you add a version number here, you *must* add an AC_SUBST line for
dnl it too, or it will never make it into the spec file!

GDK_PIXBUF_REQUIRED=2.21.3
GTK_REQUIRED=3.3.6
GLIB_REQUIRED=2.32.3
XRANDR_REQUIRED=1.3
XEXT_REQUIRED=1.1

AC_SUBST(GTK_REQUIRED)
AC_SUBST(GLIB_REQUIRED)
AC_SUBST(GDK_PIXBUF_REQUIRED)
AC_SUBST(XRANDR_REQUIRED)
AC_SUBST(XEXT_REQUIRED)

AC_CHECK_LIBM
AC_SUBST(LIBM)

dnl we need x11 for GnomeBG

PKG_CHECK_MODULES(XLIB, x11,
    X11_PACKAGE=x11,
    [X11_PACKAGE=
     AC_PATH_XTRA
     if test "x$no_x" = xyes; then
       AC_MSG_ERROR("no (requires X development libraries)")
     else
       XLIB_LIBS="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"
       XLIB_CFLAGS=$X_CFLAGS
     fi])
AC_SUBST(X11_PACKAGE)
AC_SUBST(XLIB_CFLAGS)
AC_SUBST(XLIB_LIBS)

dnl pkg-config dependency checks

PKG_CHECK_MODULES(CINNAMON_DESKTOP, gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQUIRED
                                    gtk+-3.0 >= $GTK_REQUIRED
                                    glib-2.0 >= $GLIB_REQUIRED
                                    gio-2.0 >= $GLIB_REQUIRED
                                    xrandr >= $XRANDR_REQUIRED
                                    xext >= $XEXT_REQUIRED
                                    xkeyboard-config
                                    xkbfile)

XKB_BASE=$($PKG_CONFIG --variable xkb_base xkeyboard-config)
AC_SUBST(XKB_BASE)

AC_CACHE_CHECK(for timerfd_create(2) system call,
    gnome_cv_timerfd,AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <sys/timerfd.h>
#include <unistd.h>
],[
int
main (void)
{
  timerfd_create (CLOCK_MONOTONIC, TFD_CLOEXEC);
  return 0;
}
])],gnome_cv_timerfd=yes,gnome_cv_timerfd=no))
if test x"$gnome_cv_timerfd" = x"yes"; then
  AC_DEFINE(HAVE_TIMERFD, 1, [we have the timerfd_create(2) system call])
fi

dnl Language Support

GETTEXT_PACKAGE=cinnamon-desktop
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
		   [The gettext translation domain])
AC_SUBST(GETTEXT_PACKAGE)

AM_GLIB_GNU_GETTEXT

GLIB_GSETTINGS

GOBJECT_INTROSPECTION_CHECK([0.9.7])

AC_CONFIG_FILES([
Makefile
libcinnamon-desktop/Makefile
libcinnamon-desktop/cinnamon-desktop.pc
libcinnamon-desktop/cinnamon-desktop-uninstalled.pc
schemas/Makefile
po/Makefile.in
])

AC_OUTPUT

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
              
        prefix:                       ${prefix}
        exec_prefix:                  ${exec_prefix}
        libdir:                       ${libdir}
        bindir:                       ${bindir}
        sbindir:                      ${sbindir}
        sysconfdir:                   ${sysconfdir}
        localstatedir:                ${localstatedir}
        datadir:                      ${datadir}
        source code location:         ${srcdir}
        compiler:                     ${CC}
        cflags:                       ${CFLAGS}
        Maintainer mode:              ${USE_MAINTAINER_MODE}
        Use *_DISABLE_DEPRECATED:     ${enable_deprecation_flags}

"
