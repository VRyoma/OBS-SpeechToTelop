# Stub to satisfy OBS's find_dependency(w32-pthreads) and libobsTargets.cmake.
# libobsTargets.cmake checks for OBS::w32-pthreads; if missing it sets libobs_FOUND=FALSE.
# The plugin does not link w32-pthreads directly — obs.dll carries it at runtime.
if(NOT TARGET w32-pthreads::w32-pthreads)
  add_library(w32-pthreads::w32-pthreads INTERFACE IMPORTED)
endif()
if(NOT TARGET OBS::w32-pthreads)
  add_library(OBS::w32-pthreads INTERFACE IMPORTED)
endif()
set(w32-pthreads_FOUND TRUE)
set(w32-pthreads_LIBRARIES w32-pthreads::w32-pthreads)
