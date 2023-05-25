cmake_minimum_required(VERSION 3.26.4)

project(urbit)

add_executable(
  ${PROJECT_NAME}
)

subdirs(
  pkg/c3
  pkg/ent
  pkg/noun
  pkg/ur
  pkg/urcrypt
  pkg/vere
)

target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC
  c3
  ent
  noun
  ur
  urcrypt
  vere
)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
  pkg/c3
  pkg/ent
  pkg/noun
  pkg/ur
  pkg/urcrypt
  pkg/vere
)

# specify source files
target_sources(
  ${PROJECT_NAME}
  PUBLIC
  pkg/vere/main.c
)