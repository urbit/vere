add_library(
  ent
  STATIC
  ent.c
  ent.h
)

# define copts
target_compile_options(
  ent
  PUBLIC
  -O3
  -Wall
  -Wextra
  -Wpedantic
  -Werror
  -std=gnu99
)

# define macros for different platforms
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_compile_definitions(
    ent
    PUBLIC
    ENT_GETENTROPY_SYSRANDOM
  )
else()
  target_compile_definitions(
    ent
    PUBLIC
    ENT_GETENTROPY_UNISTD
  )
endif()