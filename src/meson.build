clippy_sources = [
  'utils.c',
  'clippy.c',
]

gtk_name = 'gtk+-3.0'
gtk_dep = dependency(gtk_name, version: '>= 3.22')

clippy_deps = [
  gtk_dep,
  dependency('gmodule-2.0'),
]

gnome = import('gnome')

clippy_sources += gnome.compile_resources(
    'clippy-resources', 'clippy.gresource.xml',
    c_name: 'clippy'
)

gtk_modules_path = join_paths(gtk_name,
                              gtk_dep.get_pkgconfig_variable('gtk_binary_version'),
                              'modules')

clippy_lib = shared_module(
  'clippy-module',
  clippy_sources,
  dependencies: clippy_deps,
  install: true,
  install_dir: gtk_modules_path
)
