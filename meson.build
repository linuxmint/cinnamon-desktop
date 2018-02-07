# Meson build file

# https://github.com/linuxmint/cinnamon-desktop
project('cinnamon-desktop', 'c', version: '3.6.2',
  meson_version: '>=0.37.0'
)

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

if not get_option('deprecation_flags')
  add_global_arguments('-Wno-deprecated-declarations', language: 'c')
endif

################################################################################

math    = cc.find_library('m')

accserv = dependency('accountsservice')
alsa    = dependency('alsa',            required: false)
gdk_pixb= dependency('gdk-pixbuf-2.0',  version: '>=2.22.0')
gio     = dependency('gio-2.0',         version: '>=2.37.3')
glib    = dependency('glib-2.0',        version: '>=2.37.3')
gtk     = dependency('gtk+-3.0',        version: '>=3.3.16')

x11     = dependency('x11')
xext    = dependency('xext',            version: '>=1.1')
xkbconf = dependency('xkeyboard-config')
xkbfile = dependency('xkbfile')
xrandr  = dependency('xrandr',          version: '>=1.3')

cinnamon_deps = [
  accserv,
  gdk_pixb,
  gio,
  glib,
  gtk,
  math,
  x11,
  xext,
  xkbconf,
  xkbfile,
  xrandr,
]

# CVC dependencies
gobject = dependency('gobject-2.0')
pulse   = dependency('libpulse')
pulsegl = dependency('libpulse-mainloop-glib')

cvc_deps = cinnamon_deps + [
  pulse,
  pulsegl,
  gobject,
  alsa,
]

xkb_base = xkbconf.get_pkgconfig_variable('xkb_base')


# Path to the pnp.ids file -- to know if we use one shipped with another
# package, or an internal file

pnp_ids_path = get_option('pnp-ids-path')
pnp_ids_install_internal = (pnp_ids_path == '')

if pnp_ids_install_internal
  # Default value
  pnp_ids_path = join_paths(get_option('datadir'), 'libcinnamon-desktop')
  pnp_ids_abspath = join_paths(get_option('prefix'), pnp_ids_path)
else
  pnp_ids_abspath = pnp_ids_path
endif

################################################################################
# Config

timerfd_check = cc.compiles('''
#include <sys/timerfd.h>
#include <unistd.h>
int main () {
  timerfd_create (CLOCK_MONOTONIC, TFD_CLOEXEC);
  return 0;
}
''')

gettext_package = meson.project_name()
conf.set_quoted('GETTEXT_PACKAGE', gettext_package)
conf.set_quoted('GNOMELOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

conf.set('HAVE_BIND_TEXTDOMAIN_CODESET', cc.has_function('bind_textdomain_codeset'))
conf.set('datadir',             get_option('datadir'))
conf.set('ENABLE_NLS',          cc.has_header('libintl.h'))
conf.set('HAVE_ALSA',           alsa.found())
conf.set('HAVE_GETTEXT',        true)
conf.set('HAVE_INTROSPECTION',  true)
conf.set('HAVE_TIMERFD',        timerfd_check)
conf.set('ACCOUNTSERVICE_BG', cc.has_function('accounts_user_set_background_file', dependencies: accserv))

################################################################################

rootInclude = include_directories('.')

configure_file(
  output: 'config.h',
  configuration: conf
)

subdir('install-scripts')
subdir('po')
subdir('files')
subdir('libcinnamon-desktop')
subdir('libcvc')
subdir('schemas')


pnp_message = '@0@: @1@'.format(
  ''+(pnp_ids_install_internal ? 'internal' : 'system'),
  pnp_ids_abspath
)

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
  '        source code location:         ' + meson.source_root(),
  '        compiler:                     ' + cc.get_id(),
  '        debugging support:            ' + get_option('buildtype'),
  '        Use *_DISABLE_DEPRECATED:     @0@'.format(get_option('deprecation_flags')),
  '        Use PNP files:                ' + pnp_message,
  '',
]))
