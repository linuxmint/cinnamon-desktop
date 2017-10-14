libcvc_gir_sources = [
  'gvc-channel-map.c',
  'gvc-mixer-card.c',
  'gvc-mixer-control.c',
  'gvc-mixer-event-role.c',
  'gvc-mixer-event-role.h',
  'gvc-mixer-sink-input.c',
  'gvc-mixer-sink.c',
  'gvc-mixer-sink.h',
  'gvc-mixer-source-output.c',
  'gvc-mixer-source.c',
  'gvc-mixer-stream.c',
  'gvc-mixer-ui-device.c',
]
libcvc_sources = [
  'gvc-channel-map-private.h',
  'gvc-mixer-card-private.h',
  'gvc-mixer-control-private.h',
  'gvc-mixer-stream-private.h',
  'gvc-pulseaudio-fake.h',
  libcvc_gir_sources,
]

libcvc_headers = [
  'gvc-channel-map.h',
  'gvc-mixer-card.h',
  'gvc-mixer-control.h',
  'gvc-mixer-sink-input.h',
  'gvc-mixer-source-output.h',
  'gvc-mixer-source.h',
  'gvc-mixer-stream.h',
  'gvc-mixer-ui-device.h',
]

libcvc = shared_library('cvc',
  libcvc_sources,
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
  sources: libcvc_gir_sources + libcvc_headers,
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

pkgconfig.generate(
  name: 'cvc',
  description: 'Utility library for volume control (based on gvc)',
  version: '0.0',
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
