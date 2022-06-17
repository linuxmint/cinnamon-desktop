libcvc_gir_sources = [
  'gvc-channel-map.c',
  'gvc-mixer-card.c',
  'gvc-mixer-control.c',
  'gvc-mixer-event-role.c',
  'gvc-mixer-sink.c',
  'gvc-mixer-sink-input.c',
  'gvc-mixer-source.c',
  'gvc-mixer-source-output.c',
  'gvc-mixer-stream.c',
  'gvc-mixer-ui-device.c'
]

libcvc_sources = [
  'gvc-mixer-card-private.h',
  'gvc-mixer-stream-private.h',
  'gvc-channel-map-private.h',
  'gvc-mixer-control-private.h',
  libcvc_gir_sources,
]

cvc_deps = [
  dependency('gio-2.0'),
  dependency('gobject-2.0'),
  dependency('libpulse', version: '>= 12.99.3'),
  dependency('libpulse-mainloop-glib')
]

if use_alsa
  cvc_deps += dependency('alsa')
endif

libcvc_headers = [
  'gvc-channel-map.h',
  'gvc-mixer-card.h',
  'gvc-mixer-control.h',
  'gvc-mixer-event-role.h',
  'gvc-mixer-sink.h',
  'gvc-mixer-sink-input.h',
  'gvc-mixer-source.h',
  'gvc-mixer-source-output.h',
  'gvc-mixer-stream.h',
  'gvc-mixer-ui-device.h',
]

libcvc_enums = gnome.mkenums_simple('gvc-enum-types',
  sources: libcvc_headers
)

libcvc = shared_library('cvc',
  libcvc_sources + libcvc_enums,
  include_directories: rootInclude,
  dependencies: cvc_deps,
  c_args: [
    '-DG_LOG_DOMAIN="Cvc"',
    '-DWITH_INTROSPECTION',
  ],
  install: true,
  version: '0.0.0',
)

gnome.generate_gir(libcvc,
  sources: libcvc_gir_sources + libcvc_headers + libcvc_enums,
  namespace: 'Cvc',
  nsversion: '1.0',
  identifier_prefix: 'Gvc',
  symbol_prefix: 'gvc_',
  includes: [
    'GObject-2.0',
    'Gio-2.0',
  ],
  install: true,
)


install_headers(
  libcvc_headers,
  subdir: join_paths('cinnamon-desktop', 'libcvc'),
)

if use_alsa
  executable('test-audio-device-selection',
    sources: 'test-audio-device-selection.c',
    link_with: libcvc,
    dependencies: cvc_deps
  )
endif

pkgconfig.generate(
  name: 'cvc',
  description: 'Utility library for volume control (based on gvc)',
  version: meson.project_version(),
  requires: [
    'gio-2.0',
    'libpulse',
    'libpulse-mainloop-glib',
    'gobject-2.0',
  ],
  libraries: libcvc,
  subdirs: join_paths('cinnamon-desktop', 'libcvc'),
  variables: [
    'exec_prefix=${prefix}',
  ],
)
