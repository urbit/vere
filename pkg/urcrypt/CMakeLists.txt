add_subdirectory(ge-additions)

add_library(
  urcrypt
  STATIC
  aes_cbc.c
  aes_ecb.c
  aes_siv.c
  argon.c
  ed25519.c
  ge_additions.c
  keccak.c
  ripemd.c
  scrypt.c
  secp256k1.c
  sha.c
  util.c
  util.h
)

target_include_directories(
  urcrypt
  PUBLIC
  ${aes_siv_SOURCE_DIR}
  ${argon2_SOURCE_DIR}/include
  ${argon2_SOURCE_DIR}/src/blake2
  ${ed25519_SOURCE_DIR}/src
  ${ge_additions_SOURCE_DIR}
  ${keccak_tiny_SOURCE_DIR}
  ${scrypt_SOURCE_DIR}
  ${secp256k1_SOURCE_DIR}/include
  ${openssl_SOURCE_DIR}/include
)

add_dependencies(
  urcrypt
  ge-additions
  aes_siv
  argon2
  ed25519
  keccak_tiny
  # openssj
  scrypt
  # secp256k1
)
