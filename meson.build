project(
  'logspine',
  'cpp',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'cpp_std=c++23,c++latest',
    'warning_level=3',
    'werror=true',
  ],
)

cpp = meson.get_compiler('cpp')

developer_mode = get_option('developer_mode')
enable_werror = developer_mode and get_option('werror')
network_enabled = get_option('network')

project_warnings = []
if cpp.get_id() == 'msvc'
  project_warnings += ['/W4', '/permissive-']
  if enable_werror
    project_warnings += ['/WX']
  endif
else
  project_warnings += [
    '-Wall',
    '-Wextra',
    '-Wpedantic',
    '-Wconversion',
    '-Wshadow',
  ]
  if enable_werror
    project_warnings += ['-Werror']
  endif
endif

inc = include_directories('include')
private_inc = include_directories('src')

public_headers = files(
  'include/logspine/async_dispatcher.hpp',
  'include/logspine/config.hpp',
  'include/logspine/dispatcher.hpp',
  'include/logspine/field.hpp',
  'include/logspine/mdc.hpp',
  'include/logspine/filter.hpp',
  'include/logspine/formatter.hpp',
  'include/logspine/logger.hpp',
  'include/logspine/logspine.hpp',
  'include/logspine/noop_sink.hpp',
  'include/logspine/registry.hpp',
  'include/logspine/sink.hpp',
  'include/logspine/source_location.hpp',
  'include/logspine/sync_dispatcher.hpp',
  'include/logspine/sinks/console_sink.hpp',
  'include/logspine/sinks/elastic_bulk_file_sink.hpp',
  'include/logspine/sinks/file_sink.hpp',
  'include/logspine/sinks/gelf_udp_sink.hpp',
  'include/logspine/sinks/network_sink_statistics.hpp',
  'include/logspine/sinks/tcp_json_lines_sink.hpp',
  'include/logspine/sinks/otlp_http_sink.hpp',
)

logspine_sources = files(
  'src/async_dispatcher.cpp',
  'src/field.cpp',
  'src/json.cpp',
  'src/level.cpp',
  'src/logger.cpp',
  'src/mdc.cpp',
  'src/zlib_helper.cpp',
  'src/net/socket.cpp',
  'src/registry.cpp',
  'src/sync_dispatcher.cpp',
  'src/sinks/console_sink.cpp',
  'src/sinks/elastic_bulk_file_sink.cpp',
  'src/sinks/file_sink.cpp',
  'src/sinks/gelf_udp_sink.cpp',
  'src/sinks/network_payloads.cpp',
  'src/sinks/tcp_json_lines_sink.cpp',
  'src/sinks/otlp_http_sink.cpp',
)

logspine_cpp_args = project_warnings
if network_enabled
  logspine_cpp_args += ['-DLOGSPINE_WITH_NETWORK=1']
endif

zlib_dep = dependency('zlib', required: false)
logspine_deps = []
if zlib_dep.found()
  logspine_deps += zlib_dep
  logspine_cpp_args += ['-DLOGSPINE_WITH_ZLIB=1']
endif

logspine_lib = library(
  'logspine',
  logspine_sources,
  cpp_args: logspine_cpp_args,
  dependencies: logspine_deps,
  include_directories: [inc, private_inc],
  install: true,
)

install_subdir('include/logspine', install_dir: get_option('includedir'))

logspine_dep = declare_dependency(
  include_directories: inc,
  link_with: logspine_lib,
)

if get_option('build_examples')
  examples = {
    'logspine_basic_example': 'examples/basic.cpp',
    'logspine_elastic_bulk_example': 'examples/elastic_bulk_file.cpp',
    'logspine_structured_example': 'examples/structured.cpp',
  }
  if network_enabled
    examples += {'logspine_graylog_gelf_udp_example': 'examples/graylog_gelf_udp.cpp'}
  endif
  foreach name, source : examples
    executable(
      name,
      source,
      dependencies: logspine_dep,
      cpp_args: project_warnings,
      include_directories: [inc, private_inc],
    )
  endforeach
endif

if get_option('build_tests')
  catch2_proj = subproject(
    'catch2',
    default_options: [
      'tests=false',
      'install=false',
    ],
  )
  catch2_dep = catch2_proj.get_variable('catch2_with_main_dep')
  tests = {
    'test_core': 'tests/test_core.cpp',
    'test_json': 'tests/test_json.cpp',
    'test_async_dispatcher': 'tests/test_async_dispatcher.cpp',
    'test_sinks': 'tests/test_sinks.cpp',
    'test_mdc': 'tests/test_mdc.cpp',
    'test_format': 'tests/test_format.cpp',
    'test_otlp': 'tests/test_otlp.cpp',
  }
  if network_enabled
    tests += {'test_network_sinks': 'tests/test_network_sinks.cpp'}
  endif
  foreach name, source : tests
    test_exe = executable(
      name,
      source,
      dependencies: [logspine_dep, catch2_dep],
      cpp_args: project_warnings,
      include_directories: [inc, private_inc],
    )
    test(name, test_exe)
  endforeach
endif

if get_option('build_benchmarks')
  executable(
    'bench_logging',
    'benchmarks/bench_logging.cpp',
    dependencies: logspine_dep,
    cpp_args: project_warnings,
    include_directories: [inc, private_inc],
  )
endif

clang_format = find_program('clang-format', required: false)
format_targets = public_headers + logspine_sources + files(
  'examples/basic.cpp',
  'examples/elastic_bulk_file.cpp',
  'examples/graylog_gelf_udp.cpp',
  'examples/structured.cpp',
  'tests/test_async_dispatcher.cpp',
  'tests/test_core.cpp',
  'tests/test_json.cpp',
  'tests/test_network_sinks.cpp',
  'tests/test_sinks.cpp',
)
if clang_format.found()
  run_target('format', command: [clang_format, '-i'] + format_targets)
  run_target('format-check', command: [clang_format, '--dry-run', '--Werror'] + format_targets)
endif

clang_tidy = find_program('clang-tidy', required: false)
if clang_tidy.found()
  run_target('clang-tidy', command: [clang_tidy, '-p', meson.current_build_dir()] + logspine_sources)
endif
