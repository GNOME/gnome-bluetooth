name = 'bluetooth-sendto'

executable(
  name,
  'main.c',
  include_directories: top_inc,
  dependencies: [libgnome_bluetooth_dep, gtk_dep, libadwaita_dep],
  install: true,
)

install_man(name + '.1')

desktop = name + '.desktop'

i18n.merge_file (
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: gnomebt_datadir / 'applications',
)
