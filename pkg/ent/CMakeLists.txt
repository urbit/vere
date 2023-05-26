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
    # TODO: support fallback to other options if `getrandom()` isn't
    # available in `unistd.h`. Preferred order (from most 
    # preferred to least preferred) is:
    # - ENT_GETENTROPY_UNISTD
    # - ENT_GETENTROPY_SYSRANDOM
    # - ENT_GETRANDOM_SYSCALL
    # - ENT_DEV_URANDOM
    ENT_GETENTROPY_SYSRANDOM
  )
else()
  target_compile_definitions(
    ent
    PUBLIC
    ENT_GETENTROPY_UNISTD
  )
endif()