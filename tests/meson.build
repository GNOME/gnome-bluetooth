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
