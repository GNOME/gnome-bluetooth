lib_inc = include_directories('.')

enum_headers = files('bluetooth-enums.h')

headers = enum_headers + files(
  'bluetooth-device.h',
  'bluetooth-client.h',
  'bluetooth-utils.h',
)
ui_headers = files(
  'bluetooth-settings-widget.h',
)

install_headers(
  ui_headers,
  subdir: gnomebt_api_name,
)

sources = files(
  'bluetooth-agent.c',
  'bluetooth-client.c',
  'bluetooth-device.c',
  'bluetooth-utils.c',
  'pin.c',
)

ui_sources = sources + files(
  'bluetooth-pairing-dialog.c',
  'bluetooth-settings-obexpush.c',
  'bluetooth-settings-row.c',
  'bluetooth-settings-widget.c',
)

resource_data = files(
  'bluetooth-pairing-dialog.ui',
  'bluetooth-settings.css',
  'bluetooth-settings-row.ui',
  'settings.ui',
)

ui_built_sources = gnome.compile_resources(
  'bluetooth-settings-resources',
  'bluetooth.gresource.xml',
  c_name: 'bluetooth_settings',
  dependencies: resource_data,
  export: true,
)

enum_sources = gnome.mkenums_simple(
  'gnome-bluetooth-enum-types',
  sources : enum_headers,
)

client = 'bluetooth-client'
client_built_sources = gnome.gdbus_codegen(
  client + '-glue',
  client + '.xml',
  interface_prefix: 'org.bluez',
)

deps = [
  gio_dep,
]

private_deps = [
  gio_unix_dep,
  libudev_dep,
  m_dep,
  upower_dep,
]

ui_deps = [
  gio_dep,
  gtk_dep,
  libadwaita_dep,
]

ui_private_deps = [
  gio_unix_dep,
  gsound_dep,
  libnotify_dep,
  libudev_dep,
  upower_dep,
  m_dep,
]

cflags = [
  '-DPKGDATADIR="@0@"'.format(gnomebt_prefix / gnomebt_pkgdatadir),
  '-DG_LOG_DOMAIN="Bluetooth"',
]

symbol_map = meson.current_source_dir() / (meson.project_name() + '.map')
ldflags = cc.get_supported_link_arguments('-Wl,--version-script,' + symbol_map)

libgnome_bluetooth = shared_library(
  gnomebt_api_name,
  sources: sources + enum_sources + client_built_sources,
  version: libversion,
  include_directories: top_inc,
  dependencies: deps + private_deps,
  c_args: cflags,
  link_args: ldflags,
  link_depends: symbol_map,
  install: true,
)

libgnome_bluetooth_dep = declare_dependency(
  link_with: libgnome_bluetooth,
  include_directories: lib_inc,
  dependencies: deps,
)

pkg.generate(
  libraries: libgnome_bluetooth,
  version: gnomebt_version,
  name: gnomebt_api_name,
  description: 'Bluetooth libraries for gnome-shell',
  filebase: gnomebt_api_name,
  subdirs: gnomebt_api_name,
  requires: 'gio-2.0',
  variables: 'exec_prefix=${prefix}',
)

if enable_gir
  gir_sources = sources + headers + enum_sources

  gir_incs = [
    'Gio-2.0',
  ]

  gnome.generate_gir(
    libgnome_bluetooth,
    sources: gir_sources,
    nsversion: gnomebt_api_version,
    namespace: gnomebt_gir_ns,
    symbol_prefix: 'bluetooth',
    identifier_prefix: 'Bluetooth',
    export_packages: gnomebt_api_name,
    includes: gir_incs,
    install: true,
  )

  gnomebt_priv_gir = gnome.generate_gir(
    libgnome_bluetooth,
    sources: gir_sources + [
      'bluetooth-agent.h',
      'bluetooth-client-private.h',
      ],
    nsversion: gnomebt_api_version,
    namespace: 'GnomeBluetoothPriv',
    symbol_prefix: 'bluetooth',
    identifier_prefix: 'Bluetooth',
    export_packages: gnomebt_api_name,
    includes: gir_incs,
    install: false,
  )
endif

ui_symbol_map = meson.current_source_dir() / (meson.project_name() + '-ui.map')
ui_ldflags = cc.get_supported_link_arguments('-Wl,--version-script,' + ui_symbol_map)

libgnome_bluetooth_ui = shared_library(
  gnomebt_ui_api_name,
  sources: ui_sources + enum_sources + client_built_sources + ui_built_sources,
  version: libversion,
  include_directories: top_inc,
  dependencies: ui_deps + ui_private_deps,
  c_args: cflags,
  link_args: ui_ldflags,
  link_depends: ui_symbol_map,
  install: true,
)

libgnome_bluetooth_ui_dep = declare_dependency(
  link_with: libgnome_bluetooth_ui,
  include_directories: lib_inc,
  dependencies: ui_deps,
)

pkg.generate(
  libraries: libgnome_bluetooth_ui,
  version: gnomebt_version,
  name: gnomebt_ui_api_name,
  description: 'Bluetooth libraries for gnome-control-center',
  filebase: gnomebt_ui_api_name,
  subdirs: gnomebt_api_name,
  requires: [ 'gio-2.0', 'gtk4', 'libadwaita-1' ],
  variables: 'exec_prefix=${prefix}',
)

test_names = [
  'test-agent',
  'test-class',
  'test-liststore',
  'test-pairing-dialog',
  'test-pin',
  'test-settings',
]

foreach name: test_names
  executable(
    name,
    [name + '.c'] + ui_sources + ui_built_sources + enum_sources + client_built_sources,
    include_directories: top_inc,
    dependencies: ui_deps + ui_private_deps,
    c_args: cflags,
  )
endforeach

database = 'pin-code-database.xml'

install_data(
  database,
  install_dir: gnomebt_pkgdatadir,
)

custom_target(
  database,
  input: database,
  output: database + '.valid',
  command: [find_program('xmllint'), '--output', '@OUTPUT@', '--valid', '@INPUT'],
)
