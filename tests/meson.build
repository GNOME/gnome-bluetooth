if enable_gir and has_dbusmock
  c_execs = []
  c_exec = executable('test_battery',
    'test_battery.c',
    include_directories: lib_inc,
    dependencies: deps,
    c_args: cflags,
    link_with: libgnome_bluetooth,
  )
  c_execs += [c_exec]

  integration_test = find_program('integration-test.py')

  envs = environment()
  envs.set ('top_builddir', meson.global_build_root())
  envs.set ('top_srcdir', meson.global_source_root())
  envs.set ('GI_TYPELIB_PATH', meson.global_build_root() / 'lib')

  test_deps = [ gnomebt_priv_gir, c_exec ]

  python3 = find_program('python3')
  unittest_inspector = find_program('unittest_inspector.py')
  r = run_command(unittest_inspector, files('integration-test.py'), check: true)
  unit_tests = r.stdout().strip().split('\n')

  foreach ut: unit_tests
    ut_args = files('integration-test.py')
    ut_args += ut
    test(ut,
      python3,
      args: ut_args,
      env: envs,
      depends: test_deps
    )
  endforeach
endif

if not has_dbusmock
  test('integration-test-skipped',
    find_program('integration-test-skipped.sh'),
  )
endif

test_bluetooth_device = executable('test-bluetooth-device',
  'test-bluetooth-device.c',
  include_directories: lib_inc,
  dependencies: deps,
  c_args: cflags,
  link_with: libgnome_bluetooth,
)

test('test-bluetooth-device-test',
  test_bluetooth_device,
)

test_bluetooth_utils = executable('test-bluetooth-utils',
  [ 'test-bluetooth-utils.c', enum_sources ],
  include_directories: [ top_inc, lib_inc],
  dependencies: deps,
  c_args: cflags,
  link_with: libgnome_bluetooth,
)

test('test-bluetooth-device-utils',
  test_bluetooth_utils,
)
