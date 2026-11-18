configure_file(
  input: 'config.hpp.in',
  output: 'config.hpp',
  configuration: {'LOGSPINE_CONFIG_NETWORK': network_enabled.to_int()},
  install_dir: get_option('includedir') / 'logspine',
)
