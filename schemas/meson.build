desktop_gschemas = [
  'a11y.applications',
  'a11y.keyboard',
  'a11y.magnifier',
  'a11y.mouse',
  'background',
  'default-applications',
  'input-sources',
  'interface',
  'keybindings.media-keys',
  'keybindings.wm',
  'keybindings',
  'lockdown',
  'media-handling',
  'notifications',
  'privacy',
  'screensaver',
  'session',
  'sound',
  'thumbnail-cache',
  'thumbnailers',
  'wm.preferences',
]

gschema_conf = configuration_data()
gschema_conf.set('datadir', join_paths(get_option('prefix'), get_option('datadir')))
gschema_conf.set('GETTEXT_PACKAGE', gettext_package)

foreach schema : desktop_gschemas
  schema_file   = 'org.cinnamon.desktop.' + schema + '.gschema.xml'

  schema_configured = configure_file(
    input : schema_file + '.in.in',
    output: schema_file + '.in',
    configuration : gschema_conf,
    install : false,
  )

  schema_translated = custom_target(schema_file,
    input : schema_configured,
    output: schema_file,
    command : ['intltool-merge', '--quiet', '--xml-style', '--utf8', po_subdir, '@INPUT@', '@OUTPUT@'],
    install : true,
    install_dir : join_paths(get_option('datadir'), 'glib-2.0', 'schemas'),
    build_always : true,
  )

endforeach
