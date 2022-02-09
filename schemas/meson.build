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
  'peripherals',
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

foreach schema : desktop_gschemas
  schema_file = 'org.cinnamon.desktop.' + schema + '.gschema.xml'

  schema_configured = configure_file(
    input : schema_file + '.in',
    output: schema_file,
    configuration : gschema_conf,
    install : true,
    install_dir : get_option('datadir') / 'glib-2.0' / 'schemas'
  )

endforeach
