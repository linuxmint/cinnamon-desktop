subdir('libgsystem')


libcinnamon_desktop_gir_sources = [
  'display-name.c',
  'edid-parse.c',
  'gnome-bg-crossfade.c',
  'gnome-bg.c',
  'gnome-desktop-thumbnail.c',
  'gnome-desktop-utils.c',
  'gnome-installer.c',
  'gnome-pnp-ids.c',
  'gnome-rr-config.c',
  'gnome-rr-labeler.c',
  'gnome-rr-output-info.c',
  'gnome-rr.c',
  'gnome-thumbnail-pixbuf-utils.c',
  'gnome-wall-clock.c',
  'gnome-xkb-info.c',
]

libcinnamon_desktop_sources = [
  'gnome-datetime-source.c',
  libcinnamon_desktop_gir_sources,
]

cdesktop_headers = [
  'cdesktop-enums.h',
]
libcinnamon_desktop_headers = [
  'gnome-bg-crossfade.h',
  'gnome-bg.h',
  'gnome-desktop-thumbnail.h',
  'gnome-desktop-utils.h',
  'gnome-installer.h',
  'gnome-pnp-ids.h',
  'gnome-rr-config.h',
  'gnome-rr-labeler.h',
  'gnome-rr.h',
  'gnome-wall-clock.h',
  'gnome-xkb-info.h',
]

libcinnamon_desktop = shared_library('cinnamon-desktop',
  libcinnamon_desktop_sources,
  include_directories: [ rootInclude, libgsystem_include, ],
  c_args: [
    '-DG_LOG_DOMAIN="CinnamonDesktop"',
    '-DPNP_IDS="@0@"'.format(pnp_ids_path),
    '-DXKB_BASE="@0@"'.format(xkb_base),
  ],
  dependencies: cinnamon_deps,
  link_whole: libgsystem,
  install: true,
  version: LT_VERSION,
)

install_headers(
  libcinnamon_desktop_headers + cdesktop_headers,
  subdir: join_paths('cinnamon-desktop', 'libcinnamon-desktop')
)

pkgconfig.generate(
  name: 'cinnamon-desktop',
  description: 'Utility library for loading.desktop files',
  version: LT_VERSION,
  requires: [ 'gtk+-3.0', ],
  requires_private: [ 'xkbfile', ],
  libraries: libcinnamon_desktop,
  subdirs: join_paths('cinnamon-desktop'),
  variables: [ 'exec_prefix=${prefix}'],
)

if pnp_ids_install_internal
  install_data('pnp.ids',
    install_dir: pnp_ids_path,
  )
endif


gir_c_includes = []
foreach hdr : libcinnamon_desktop_headers
  gir_c_includes += '--c-include=libcinnamon-desktop/' + hdr
endforeach

cdesktop_gir = gnome.generate_gir(libcinnamon_desktop,
  sources: 'cdesktop-enums.h',
  namespace: 'CDesktopEnums',
  nsversion: '3.0',
  identifier_prefix: 'CDesktop',
  symbol_prefix: 'c_desktop',
  extra_args: [
    '-DGNOME_DESKTOP_USE_UNSTABLE_API',
  ] + gir_c_includes,
  install: true,
)
gnome.generate_gir(libcinnamon_desktop,
  sources: libcinnamon_desktop_gir_sources + libcinnamon_desktop_headers,
  namespace: 'CinnamonDesktop',
  nsversion: '3.0',
  export_packages: 'cinnamon-desktop',
  identifier_prefix: 'Gnome',
  symbol_prefix: 'gnome_',
  includes: ['GObject-2.0', 'Gtk-3.0', cdesktop_gir[0], ],
  extra_args: [
    '-DGNOME_DESKTOP_USE_UNSTABLE_API',
  ] + gir_c_includes,
  install: true,
)

custom_target('gsettings-enums',
  input : libcinnamon_desktop_headers,
  output: 'org.cinnamon.desktop.enums.xml',
  capture: true,
  command: ['glib-mkenums',
    '--comments', '<!-- @comment@ -->',
    '--fhead', '<schemalist>',
    '--vhead', '  <@type@ id=\'org.cinnamon.desktop.@EnumName@\'>',
    '--vprod', '    <value nick=\'@valuenick@\' value=\'@valuenum@\'/>',
    '--vtail', '  </@type@>',
    '--ftail', '</schemalist>',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'),
)
