cmake_minimum_required(VERSION 3.16)
project(kvmserverguest C)

add_library(kvmserverguest SHARED
  libkvmserver.c
)

set_target_properties(kvmserverguest PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

add_custom_command(OUTPUT _binary_libkvmserverguest_so.o
  COMMAND ld -r -b binary -z noexecstack -o _binary_libkvmserverguest_so.o libkvmserverguest.so
  DEPENDS kvmserverguest
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  VERBATIM
)

SET_SOURCE_FILES_PROPERTIES(_binary_libkvmserverguest_so.o PROPERTIES
  EXTERNAL_OBJECT true
)

add_library(_binary_libkvmserverguest_so STATIC
  _binary_libkvmserverguest_so.o
)

SET_TARGET_PROPERTIES(_binary_libkvmserverguest_so PROPERTIES
  LINKER_LANGUAGE C 
)
