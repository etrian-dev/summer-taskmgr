# Meson build file for task summer taskmanager
project('summmer-taskmanager', 'c', license: 'GNU-General-Public-License-v3.0-or-later')
# list all source files
all_sources = files(
  'main.c', 'sighandlers.c', 'update_threads.c', 'utilities.c',
  'cpu_info.c', 'mem_info.c', 'process_info.c', 'process_sorting.c', 
  'windows.c')
# list dependencies that can be found with pkg-config
deps = [
  dependency('ncurses'), 
  dependency('jansson'), 
  dependency('glib-2.0'),
  #yes, thread support is enabled this way. It will find the appropriate threading library
  dependency('threads')]
# Find those libraries that need to be linked, but are not found by pkg-config or CMake
# In this case it's lm (math.h) and librt (posix timers)
cc = meson.get_compiler('c')
m_dep = cc.find_library('m')
timers_dep = cc.find_library('rt')
# Finally specifies the name of the executable to be produced,
# all the source files needed to build it
# and the dependencies to be satisfied
executable(
  'summer-taskmgr', 
  all_sources, 
  dependencies: [deps, m_dep, timers_dep])
