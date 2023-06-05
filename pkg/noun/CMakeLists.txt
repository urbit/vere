file(
  GLOB
  noun_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

file(
  GLOB
  jet_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/jets/*.h
)

file(
  GLOB_RECURSE
  jet_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/jets/*/*.c
)

add_library(
  noun
  STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/jets/tree.c
  ${jet_HEADERS}
  ${jet_SOURCES}
)

target_include_directories(
  noun
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${c3_SOURCE_DIR}
  ${ent_SOURCE_DIR}
  ${openssl_SOURCE_DIR}/include
  ${ur_SOURCE_DIR}
  ${urcrypt_SOURCE_DIR}
  ${gmp_SOURCE_DIR}
)

# define macros
target_compile_definitions(
  noun
  PRIVATE
  U3_GUARD_PAGE
)

add_dependencies(
  noun
  gmp
  c3
  ent
  ur
  urcrypt
  # murmur3
  # openssl
  # pdjson
  # sigsegv
  # softfloat
)