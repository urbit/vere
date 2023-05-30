add_library(
  c3
  STATIC
  defs.c
  defs.h
  motes.h
  portable.h
  types.h
)

# define macros for different platforms
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(c3 PUBLIC U3_OS_linux=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  message(STATUS "Building for macOS")
  target_compile_definitions(c3 PUBLIC U3_OS_osx=1)
endif()

# define macros for different architectures
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
  target_compile_definitions(
    c3
    PUBLIC 
    U3_CPU_aarch64=1
  )
endif()

# define universal macros
target_compile_definitions(
  c3
  PUBLIC
  U3_OS_ENDIAN_little=1
  U3_OS_PROF=1
)

# include dirs
target_include_directories(
  c3
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# link statically
target_link_libraries(c3 PUBLIC -static)

# export variables
set(c3_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)