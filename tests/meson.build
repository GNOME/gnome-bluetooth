if enable_gir
  integration_test = find_program('integration-test')

  envs = environment()
  envs.set ('top_builddir', meson.build_root())
  envs.set ('top_srcdir', meson.source_root())

  test_deps = [ gnomebt_priv_gir, ]

  test('gnome-bluetooth-integration-test',
    integration_test,
    args: [ 'Tests' ],
    env: envs,
    depends: test_deps
  )
endif

test_bluetooth_device = executable('test-bluetooth-device',
  'test-bluetooth-device.c',
  include_directories: lib_inc,
  dependencies: deps + private_deps,
  c_args: cflags,
  link_with: libgnome_bluetooth,
)

test('test-bluetooth-device-test',
  test_bluetooth_device,
)

test_bluetooth_utils = executable('test-bluetooth-utils',
  [ 'test-bluetooth-utils.c', enum_sources ],
  include_directories: [ top_inc, lib_inc],
  dependencies: deps + private_deps,
  c_args: cflags,
  link_with: libgnome_bluetooth,
)

test('test-bluetooth-device-utils',
  test_bluetooth_utils,
)
