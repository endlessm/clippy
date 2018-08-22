api_version = '0.1'

clippy_sources = [
  'utils.c',
  'clippy.c',
]

version_split = meson.project_version().split('.')
MAJOR_VERSION = version_split[0]
MINOR_VERSION = version_split[1]
MICRO_VERSION = version_split[2]

version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', MAJOR_VERSION)
version_conf.set('MINOR_VERSION', MINOR_VERSION)
version_conf.set('MICRO_VERSION', MICRO_VERSION)

configure_file(
  input: 'clippy-version.h.in',
  output: 'clippy-version.h',
  configuration: version_conf,
  install: true,
  install_dir: join_paths(get_option('includedir'), 'clippy')
)

clippy_deps = [
  dependency('gtk+-3.0', version: '>= 3.22'),
]

gnome = import('gnome')

clippy_sources += gnome.compile_resources(
    'clippy-resources', 'clippy.gresource.xml',
    c_name: 'clippy'
)

clippy_lib = shared_library('clippy-' + api_version,
  clippy_sources,
  dependencies: clippy_deps,
  install: true,
)

pkg = import('pkgconfig')

pkg.generate(
  description: 'A shared library to add com.endless.Clippy DBus iface to Gtk applications without recompiling them',
    libraries: clippy_lib,
         name: 'clippy',
     filebase: 'clippy-' + api_version,
      version: meson.project_version(),
      subdirs: 'clippy',
     requires: 'glib-2.0',
  install_dir: join_paths(get_option('libdir'), 'pkgconfig')
)
