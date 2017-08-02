icons_files = [
  ['apps', 'bluetooth.png'],
  ['status', 'bluetooth-active.png'],
  ['status', 'bluetooth-disabled.png'],
  ['status', 'bluetooth-paired.png']
]

icons_48x48_files = [
  ['apps', 'bluetooth.png'],
  ['status', 'bluetooth-active.png'],
  ['status', 'bluetooth-disabled.png']
]

icons_scalable_files = [
  ['apps', 'bluetooth.svg'],
  ['status', 'bluetooth-paired.svg']
]

icons = [
  ['16x16', icons_files],
  ['22x22', icons_files],
  ['24x24', icons_files],
  ['32x32', icons_files],
  ['48x48', icons_48x48_files],
  ['scalable', icons_scalable_files]
]

foreach icon: icons
  foreach icon_file: icon[1]
    install_data(
      '_'.join(['hicolor', icon_file[0], icon[0], icon_file[1]]),
      install_dir: join_paths(gnomebt_icondir, icon[0], icon_file[0])
    )
  endforeach
endforeach
