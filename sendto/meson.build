name = 'bluetooth-sendto'

deps = [
  gtk_dep,
  dependency('gio-2.0')
]

cflags = [
  '-DDATADIR="@0@"'.format(gnomebt_datadir),
  '-DICONDIR="@0@"'.format(gnomebt_icondir),
  '-DLOCALEDIR="@0@"'.format(gnomebt_localedir)
]

executable(
  name,
  'main.c',
  include_directories: [
    top_inc,
    lib_inc
  ],
  dependencies: deps,
  c_args: cflags,
  link_with: libgnome_bluetooth,
  install: true,
  install_dir: gnomebt_bindir
)

install_man(
  name + '.1',
  install_dir: join_paths(gnomebt_mandir, 'man1')
)

desktop_conf = configuration_data()
desktop_conf.set('VERSION', gnomebt_version)

desktop_name = name + '.desktop'

desktop_in = configure_file(
  input: desktop_name + '.in.in',
  output: desktop_name + '.in',
  configuration: desktop_conf
)

custom_target(
  desktop_name,
  input: desktop_in,
  output: desktop_name,
  command: [intltool_merge, '-d', '-u', '-c', intltool_cache, po_dir, '@INPUT@', '@OUTPUT@'],
  install: true,
  install_dir: join_paths(gnomebt_datadir, 'applications')
)
