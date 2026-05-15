if(NOT TARGET w32-pthreads::w32-pthreads)
  add_library(w32-pthreads::w32-pthreads INTERFACE IMPORTED)
endif()
