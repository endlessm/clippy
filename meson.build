project('clippy', 'c',
  version: '0.1.0',
  meson_version: '>= 0.40.0',
)

config_h = configuration_data()
configure_file(
  output: 'clippy-config.h',
  configuration: config_h,
)

subdir('src')
