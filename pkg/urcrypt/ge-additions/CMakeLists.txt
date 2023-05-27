add_library(
  ge-additions
  ge-additions.c
  ge-additions.h
)

target_include_directories(
  ge-additions
  PUBLIC
  ${ed25519_SOURCE_DIR}/src
)

set(ge_additions_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)