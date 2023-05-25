include(ExternalProject)

ExternalProject_Add(
  aes_siv
  URL https://github.com/dfoxfranke/libaes_siv/archive/9681279cfaa6e6399bb7ca3afbbc27fc2e19df4b.tar.gz
)

add_dependencies(
  aes_siv
  openssl
)