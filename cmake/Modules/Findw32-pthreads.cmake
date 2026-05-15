# Stub to satisfy OBS's libobsConfig.cmake find_dependency(w32-pthreads).
# The plugin itself does not link w32-pthreads directly; obs.dll carries it.
if(NOT TARGET w32-pthreads::w32-pthreads)
  add_library(w32-pthreads::w32-pthreads INTERFACE IMPORTED)
endif()
set(w32-pthreads_FOUND TRUE)
set(w32-pthreads_INCLUDE_DIRS "")
set(w32-pthreads_LIBRARIES w32-pthreads::w32-pthreads)
