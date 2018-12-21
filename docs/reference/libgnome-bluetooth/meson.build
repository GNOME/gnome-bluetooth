private_headers = [
  'bluetooth-agent.h',
  'bluetooth-chooser-private.h',
  'bluetooth-client-glue.h',
  'bluetooth-client-private.h',
  'bluetooth-fdo-glue.h',
  'bluetooth-settings-obexpush.h',
  'bluetooth-settings-row.h',
  'gnome-bluetooth-enum-types.h',
]

version_conf = configuration_data()
version_conf.set('VERSION', gnomebt_version)

version_xml = configure_file(
  input: 'version.xml.in',
  output: '@BASENAME@',
  configuration: version_conf,
)

gnome.gtkdoc(
  meson.project_name(),
  main_sgml: meson.project_name() + '-docs.sgml',
  src_dir: lib_inc,
  dependencies: libgnome_bluetooth_dep,
  ignore_headers: private_headers,
  gobject_typesfile: meson.project_name() + '.types',
  content_files: version_xml,
  install: true,
)
