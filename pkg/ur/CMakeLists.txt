add_library(
  ur
  STATIC
  bitstream.c
  bitstream.h
  defs.h
  hashcons.c
  hashcons.h
  serial.c
  serial.h
)

add_dependencies(
  ur
  murmur3
)

target_include_directories(
  ur
  PUBLIC
  ${murmur3_SOURCE_DIR}
)

set(ur_SOURCE_DIR ${ur_SOURCE_DIR} PARENT_SCOPE)