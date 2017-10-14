
libgsystem_include = include_directories('.')

libgsystem_sources = [
  'gsystem-file-utils.c',
  'gsystem-local-alloc.c',
]
libgsystem = static_library('gsystem',
  libgsystem_sources,
  dependencies: cinnamon_deps,
  include_directories: [ rootInclude, libgsystem_include, ],
  install: false,
)
