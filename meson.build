# Meson build file

# https://github.com/linuxmint/cinnamon-desktop
project('cinnamon-desktop', 'c', version : '6.4.1', meson_version : '>=0.56.0')

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C.R.A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1.0.A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1.0.0
# - If the interface is the same as the previous version, change to C.R+1.A
LT_VERSION='4.0.0'

################################################################################
conf = configuration_data()

pkgconfig = import('pkgconfig')
i18n      = import('i18n')
gnome     = import('gnome')

cc                = meson.get_compiler('c')

if not get_option('deprecation_warnings')
  add_global_arguments('-Wno-deprecated-declarations', language: 'c')
endif

################################################################################

math    = cc.find_library('m')

gdk_pixb= dependency('gdk-pixbuf-2.0',  version: '>=2.22.0')
gio     = dependency('gio-2.0',         version: '>=2.37.3')
glib    = dependency('glib-2.0',        version: '>=2.37.3')
gtk     = dependency('gtk+-3.0',        version: '>=3.3.16')
systemd = dependency('libsystemd',      required: get_option('systemd'))

x11     = dependency('x11')
xext    = dependency('xext',            version: '>=1.1')
xkbconf = dependency('xkeyboard-config')
xkbfile = dependency('xkbfile')
xrandr  = dependency('xrandr',          version: '>=1.3')
iso_codes = dependency('iso-codes')
udev    = dependency('libudev')

cinnamon_deps = [
  gdk_pixb,
  gio,
  glib,
  gtk,
  math,
  systemd,
  udev,
  x11,
  xext,
  xkbconf,
  xkbfile,
  xrandr,
]

use_alsa = get_option('alsa')

xkb_base = xkbconf.get_variable(pkgconfig: 'xkb_base')
iso_codes_prefix = iso_codes.get_variable(pkgconfig: 'prefix')

################################################################################
# Config


gettext_package = meson.project_name()
conf.set_quoted('GETTEXT_PACKAGE', gettext_package)
conf.set_quoted('GNOMELOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('LIBLOCALEDIR', join_paths(get_option('prefix'), 'lib', 'locale'))
conf.set_quoted('ISO_CODES_PREFIX', iso_codes_prefix)
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

conf.set('HAVE_BIND_TEXTDOMAIN_CODESET', cc.has_function('bind_textdomain_codeset'))
conf.set('datadir',             get_option('datadir'))
conf.set('ENABLE_NLS',          cc.has_header('libintl.h'))
conf.set('HAVE_ALSA',           use_alsa)
conf.set('HAVE_GETTEXT',        true)
conf.set('HAVE_INTROSPECTION',  true)
conf.set('HAVE_SYSTEMD',        systemd.found())
conf.set('HAVE_TIMERFD',        cc.has_function('timerfd_create'))
conf.set('HAVE_XLOCALE',        cc.has_header('xlocale.h'))
conf.set('HAVE_USELOCALE',      cc.has_function('uselocale'))
################################################################################

rootInclude = include_directories('.')

configure_file(
  output: 'config.h',
  configuration: conf
)

subdir('install-scripts')
subdir('po')
subdir('libcinnamon-desktop')
subdir('libcvc')
subdir('schemas')

message('\n'.join([
  '',
  '        prefix:                       ' + get_option('prefix'),
  '        exec_prefix:                  ' + get_option('prefix'),
  '        libdir:                       ' + get_option('libdir'),
  '        bindir:                       ' + get_option('bindir'),
  '        sbindir:                      ' + get_option('sbindir'),
  '        sysconfdir:                   ' + get_option('sysconfdir'),
  '        localstatedir:                ' + get_option('localstatedir'),
  '        datadir:                      ' + get_option('datadir'),
  '        source code location:         ' + meson.project_source_root(),
  '        compiler:                     ' + cc.get_id(),
  '        debugging support:            ' + get_option('buildtype'),
  '        Use *_DISABLE_DEPRECATED:     @0@'.format(get_option('deprecation_warnings')),
  '        Use ALSA:                     ' + '@0@'.format(use_alsa),
  '        systemd:                      @0@'.format(systemd.found()),
  '',
]))
