clippy_sources = [
  'utils.c',
  'clippy.c',
  'clippy-js-proxy.c',
  'webkit-marshal.c',
  'clippy-dbus-wrapper.c'
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

gtk_modules_path = get_option('gtk-modules-path')

if gtk_modules_path == ''
  gtk_modules_path = join_paths(gtk_name,
                                gtk_dep.get_pkgconfig_variable('gtk_binary_version'),
                                'modules')
endif

# This does nothing if the gtk_modules_path variable is an absolute path,
# otherwise it will be appended to the 'libdir' path
gtk_modules_path = join_paths(get_option('libdir'), gtk_modules_path)
message('GTK+3 modules directory:' + gtk_modules_path)

clippy_lib = shared_module(
  'clippy-module',
  clippy_sources,
  dependencies: clippy_deps,
  install: true,
  install_dir: gtk_modules_path
)

# Internal dependency, for examples
clippy_inc = include_directories('.')
clippy_dep = declare_dependency(link_with: clippy_lib,
                                include_directories: [ clippy_inc ],
                                dependencies: [ clippy_deps ])
