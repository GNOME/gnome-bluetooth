name = 'bluetooth-sendto'

executable(
  name,
  'main.c',
  include_directories: top_inc,
  dependencies: [libgnome_bluetooth_dep, gtk_dep],
  install: true,
)

install_man(name + '.1')

desktop_conf = configuration_data()
desktop_conf.set('VERSION', gnomebt_version)

desktop = name + '.desktop'

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: '@BASENAME@',
  configuration: desktop_conf,
)

i18n.merge_file (
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: gnomebt_datadir / 'applications',
)
