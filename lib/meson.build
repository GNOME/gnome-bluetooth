lib_inc = include_directories('.')

enum_headers = files('bluetooth-enums.h')

headers = enum_headers + files(
  'bluetooth-client.h',
  'bluetooth-settings-widget.h',
  'bluetooth-utils.h',
)

install_headers(
  headers,
  subdir: gnomebt_api_name,
)

sources = files(
  'bluetooth-agent.c',
  'bluetooth-client.c',
  'bluetooth-hdy-column.c',
  'bluetooth-pairing-dialog.c',
  'bluetooth-settings-obexpush.c',
  'bluetooth-settings-row.c',
  'bluetooth-settings-widget.c',
  'bluetooth-utils.c',
  'pin.c',
)

built_sources = []

resource_data = files(
  'bluetooth-pairing-dialog.ui',
  'bluetooth-settings.css',
  'bluetooth-settings-row.ui',
  'settings.ui',
)

built_sources += gnome.compile_resources(
  'bluetooth-settings-resources',
  'bluetooth.gresource.xml',
  c_name: 'bluetooth_settings',
  dependencies: resource_data,
  export: true,
)

enum_types = 'gnome-bluetooth-enum-types'

built_sources += gnome.mkenums(
  enum_types,
  sources: headers,
  c_template: enum_types + '.c.template',
  h_template: enum_types + '.h.template',
)

client = 'bluetooth-client'

built_sources += gnome.gdbus_codegen(
  client + '-glue',
  client + '.xml',
  interface_prefix: 'org.bluez',
)

deps = [
  gio_dep,
  gtk_dep,
]

private_deps = [
  gio_unix_dep,
  gsound_dep,
  libnotify_dep,
  libudev_dep,
]

cflags = [
  '-DPKGDATADIR="@0@"'.format(gnomebt_prefix / gnomebt_pkgdatadir),
  '-DG_LOG_DOMAIN="Bluetooth"',
]

symbol_map = meson.current_source_dir() / (meson.project_name() + '.map')
ldflags = cc.get_supported_link_arguments('-Wl,--version-script,' + symbol_map)

libgnome_bluetooth = shared_library(
  gnomebt_api_name,
  sources: sources + built_sources,
  version: libversion,
  include_directories: top_inc,
  dependencies: deps + private_deps + [m_dep],
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
  description: 'Widgets for Bluetooth device selection',
  filebase: gnomebt_api_name,
  subdirs: gnomebt_api_name,
  requires: deps,
  variables: 'exec_prefix=${prefix}',
)

if enable_gir
  gir_sources = sources + headers

  gir_incs = [
    'GModule-2.0',
    'GObject-2.0',
    'Gtk-4.0',
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
      'bluetooth-client-private.h',
      'bluetooth-agent.h',
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

test_names = [
  'test-agent',
  'test-class',
  'test-client',
  'test-pairing-dialog',
  'test-pin',
  'test-settings',
]

foreach name: test_names
  executable(
    name,
    [name + '.c'] + built_sources,
    include_directories: top_inc,
    dependencies: deps + private_deps,
    c_args: cflags,
    link_with: libgnome_bluetooth,
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
