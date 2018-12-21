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

configure_file(
  input: 'version.xml.in',
  output: '@BASENAME@',
  configuration: version_conf,
)

gnome.gtkdoc(
  meson.project_name(),
  main_sgml: meson.project_name() + '-docs.sgml',
  src_dir: top_inc,
  dependencies: libgnome_bluetooth_dep,
  scan_args: [
    '--rebuild-types',
    '--ignore-headers=' + ' '.join(private_headers),
  ],
  mkdb_args: [
    '--sgml-mode',
    '--output-format=xml',
  ],
  gobject_typesfile: meson.project_name() + '.types',
  install: true,
)
