include(FetchContent)

set(aes_siv_VERSION 9681279cfaa6e6399bb7ca3afbbc27fc2e19df4b)
FetchContent_Declare(
  aes_siv
  URL https://github.com/dfoxfranke/libaes_siv/archive/${aes_siv_VERSION}.tar.gz
  URL_HASH SHA256=1916a428dff480e06b09dc0fb1c9d849c048f838dc9b8d141452233b508f6bb1
)

set(openssl_VERSION 1.1.1t)
FetchContent_Declare(
  openssl
  URL https://www.openssl.org/source/openssl-${openssl_VERSION}.tar.gz
  URL_HASH SHA256=8dee9b24bdb1dcbf0c3d1e9b02fb8f6bf22165e807f45adeb7c9677536859d3b
  CONFIGURE_COMMAND "./config no-shared"
  BUILD_COMMAND "make build_libs && make install_dev"
)

# openssl with CMake support
# set(openssl_VERSION 1.1.1t-20230216)
# FetchContent_Declare(
#   openssl
#   URL https://github.com/janbar/openssl-cmake/archive/refs/tags/${openssl_VERSION}.tar.gz
#   URL_HASH SHA256=902f2d64feade5f5dc03d9d248aca1f69e8a2a21db9e3866507117a0fd9b57b1
# )


set(argon2_VERSION a4c1e3f7138c2e577376beb99f964cf71e1c8b1b)
FetchContent_Declare(
  argon2
  URL https://github.com/urbit/argon2/archive/${argon2_VERSION}.tar.gz
  URL_HASH SHA256=40850e6e6324be10f14228d17b617ad2074bb926eeddd6fe40ad5df833833949
)

# See https://curl.se/docs/caextract.html.
set(CA_BUNDLE_VERSION 2022-10-11)
FetchContent_Declare(
  ca_bundle
  URL https://curl.se/ca/cacert-${CA_BUNDLE_VERSION}.pem
  URL_HASH SHA256=2cff03f9efdaf52626bd1b451d700605dc1ea000c5da56bd0fc59f8f43071040
)

set(CURL_VERSION 7.85.0)
FetchContent_Declare(
  curl
  URL https://curl.se/download/curl-${CURL_VERSION}.tar.gz
  URL_HASH SHA256=78a06f918bd5fde3c4573ef4f9806f56372b32ec1829c9ec474799eeee641c27
)

set(ed25519_VERSION 7fa6712ef5d581a6981ec2b08ee623314cd1d1c4)
FetchContent_Declare(
  ed25519
  URL https://github.com/orlp/ed25519/archive/${ed25519_VERSION}.tar.gz
  URL_HASH SHA256=373923c85f61276e3cad2c0ae7a5d5cd4809ffe46c5abc1dc8276683a55782a0
)

set(gmp_VERSION 6.2.1)
# platform-specific configure flags (as a list)
set(gmp_CONFIGURE_FLAGS "")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND gmp_CONFIGURE_FLAGS "--disable-assembly")
endif()
list(APPEND gmp_CONFIGURE_FLAGS "--disable-shared")
list(APPEND gmp_CONFIGURE_FLAGS "--with-pic")
# TODO: add --host options for linux-aarch64 and linux-x86_64
list(JOIN gmp_CONFIGURE_FLAGS " " gmp_CONFIGURE_FLAGS)
message(STATUS "gmp_CONFIGURE_FLAGS: ${gmp_CONFIGURE_FLAGS}")

set(gmp_BUILD_FLAGS "")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  # TODO: learn how to drop into bash/shell
  list(APPEND gmp_BUILD_FLAGS "--jobs=8")
else()
  list(APPEND gmp_BUILD_FLAGS "--jobs=`nproc`")
endif()
message(STATUS "gmp_BUILD_FLAGS: ${gmp_BUILD_FLAGS}")

FetchContent_Declare(
  gmp
  URL https://gmplib.org/download/gmp/gmp-${gmp_VERSION}.tar.xz
  URL_HASH SHA256=fd4829912cddd12f84181c3451cc752be224643e87fac497b69edddadc49b4f2
  CONFIGURE_COMMAND "./configure ${gmp_CONFIGURE_FLAGS}"
  BUILD_COMMAND "make ${gmp_BUILD_FLAGS}"
)

set(h2o_VERSION 2.2.6)
FetchContent_Declare(
  h2o
  URL https://github.com/h2o/h2o/archive/refs/tags/v${H2O_VERSION}.tar.gz
  URL_HASH SHA256=f8cbc1b530d85ff098f6efc2c3fdbc5e29baffb30614caac59d5c710f7bda201
  PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/h2o-${H2O_VERSION}.patch
  # When bumping the version compare `CMakeLists.txt` in the `h2o` repo to
  # of the h2o build process.
)

set(IVORY_PILL_VERSION 721fa05)
FetchContent_Declare(
  ivory_pill
  URL https://github.com/urbit/urbit/blob/${IVORY_PILL_VERSION}/bin/ivory.pill?raw=true
  URL_HASH SHA256=26ff86808886beb831e4a135f478e42ce83ef4a09ad24808b3fe97248ce7a6b7
)

set(SOLID_PILL_VERSION 255fb1ca8206072f1d09425f0db61ecfe7ff5b17)
FetchContent_Declare(
  solid_pill
  URL https://github.com/urbit/urbit/raw/${SOLID_PILL_VERSION}/bin/solid.pill
  URL_HASH SHA256=8b658fcee6978e2b19004a54233cab953e77ea0bb6c3a04d1bfda4ddc6be63c5
)

set(keccak_tiny_VERSION 64b6647514212b76ae7bca0dea9b7b197d1d8186)
FetchContent_Declare(
  keccak_tiny
  URL https://github.com/coruus/keccak-tiny/archive/${keccak_tiny_VERSION}.tar.gz
  URL_HASH SHA256=6d4717f96b84805886c74bad89e911076664d992f197634fd7cdfca2ac0f62ef
  PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/keccak_tiny-${keccak_tiny_VERSION}.patch
)

set(lmdb_VERSION 0.9.29)
FetchContent_Declare(
  lmdb
  URL https://github.com/LMDB/lmdb/archive/refs/tags/LMDB_${lmdb_VERSION}.tar.gz
  URL_HASH SHA256=22054926b426c66d8f2bc22071365df6e35f3aacf19ad943bc6167d4cae3bebb
  PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/lmdb-${lmdb_VERSION}.patch
  # When bumping the version compare `libraries/liblmdb/Makefile` in the
  # accurate description of the lmdb build process.
)

set(murmur3_VERSION dae94be0c0f54a399d23ea6cbe54bca5a4e93ce4)
FetchContent_Declare(
  murmur3
  URL https://github.com/PeterScott/murmur3/archive/${murmur3_VERSION}.tar.gz
  URL_HASH SHA256=d81836605204df2db9e0c095423b2856073d1b2ef900463151d0663b7ca3164f
  # When bumping the version compare `makefile` in the `murmur3` repo to
  # of the murmur3 build process.
)

set(openssl_VERSION 1.1.1t)
FetchContent_Declare(
  openssl
  URL https://www.openssl.org/source/openssl-${openssl_VERSION}.tar.gz
  URL_HASH SHA256=8dee9b24bdb1dcbf0c3d1e9b02fb8f6bf22165e807f45adeb7c9677536859d3b
)

set(PDJSON_VERSION 67108d883061043e55d0fb13961ac1b6fc8a485c)
FetchContent_Declare(
  pdjson
  URL https://github.com/skeeto/pdjson/archive/${PDJSON_VERSION}.tar.gz
  URL_HASH SHA256=928913d44d9021d69c0d23a8f59ed67028e5abf1ae7910f79c23a7af4e779b92
  # When bumping the version compare `Makefile` in the `pdjson` repo to
  # of the pdjson build process.
)

set(scrypt_VERSION 60e585cdd752262b22ed4113eca41c0461a61608)
FetchContent_Declare(
  scrypt
  URL https://github.com/technion/libscrypt/archive/${scrypt_VERSION}.tar.gz
  URL_HASH SHA256=df681fb19b653b1a12970ebb6091bb2b58411b9e7baf01143870f6be3f099541
  # When bumping the version compare `Makefile` in the `scrypt` repo to
  # of the scrypt build process.
)

set(secp256k1_VERSION 694ce8fb2d1fd8a3d641d7c33705691d41a2a860)
FetchContent_Declare(
  secp256k1
  URL https://github.com/bitcoin-core/secp256k1/archive/${secp256k1_VERSION}.tar.gz
  URL_HASH SHA256=5f6e4a66bf8f3c318d91eacbf3262d1cd81a3fda6bb9af267b54cf38ffd44b1c
  PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1-${secp256k1_VERSION}.patch
  CONFIGURE_COMMAND "./autogen.sh && ./configure --disable-shared --enable-module-recovery --enable-module-schnorrsig --enable-static"
  BUILD_COMMAND "make"
)

set(SIGSEGV_VERSION 2.14)
FetchContent_Declare(
  sigsegv
  URL https://ftp.gnu.org/gnu/libsigsegv/libsigsegv-${SIGSEGV_VERSION}.tar.gz
  URL_HASH SHA256=cdac3941803364cf81a908499beb79c200ead60b6b5b40cad124fd1e06caa295
)

set(SOFTFLOAT_VERSION 5c06db33fc1e2130f67c045327b0ec949032df1d)
FetchContent_Declare(
  softfloat
  URL https://github.com/ucb-bar/berkeley-softfloat-3/archive/${SOFTFLOAT_VERSION}.tar.gz
  URL_HASH SHA256=15ad5841e88fe09422a8e31a0ef3fe126ecf678f52c9a3882f3373d47752aebe
  PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/softfloat-${SOFTFLOAT_VERSION}.patch
  # When bumping the version compare `build/<target>/Makefile` in the
  # accurate description of the softfloat process for *all* supported
  # `<target>`s.
)

set(SSE2NEON_VERSION 1.5.1)
FetchContent_Declare(
  sse2neon
  URL https://github.com/DLTcollab/sse2neon/archive/refs/tags/v${SSE2NEON_VERSION}.tar.gz
  URL_HASH SHA256=4001e2dfb14fcf3831211581ed83bcc83cf6a3a69f638dcbaa899044a351bb2a
)

set(URBIT_VERSION ea8fee3aa0434d4bdf1bf785e5ec346c7ecba7fd)
FetchContent_Declare(
  urbit
  URL https://github.com/urbit/urbit/archive/${URBIT_VERSION}.tar.gz
  URL_HASH SHA256=a729d8b3c438fef33f5ae8c4da8d84ebdb5af5ad028d41696a92cddc8bf362a8
  # We can't use a branch name for the `version` because each new commit
  # will change the SHA256 hash.
)

set(UV_VERSION 1.44.2)
FetchContent_Declare(
  uv
  URL_HASH SHA256=ccfcdc968c55673c6526d8270a9c8655a806ea92468afcbcabc2b16040f03cb4
  URL https://dist.libuv.org/dist/v${UV_VERSION}/libuv-v${UV_VERSION}.tar.gz
)

set(WHEREAMI_VERSION ba364cd54fd431c76c045393b6522b4bff547f50)
FetchContent_Declare(
  whereami
  URL https://github.com/gpakosz/whereami/archive/${WHEREAMI_VERSION}.tar.gz
  URL_HASH SHA256=1d8744177f37e8386ec2f6c5992592399040cb93535ed4fd253e1976f889a744
)

set(ZLIB_VERSION 1.2.13)
FetchContent_Declare(
  zlib
  URL https://www.zlib.net/zlib-${ZLIB_VERSION}.tar.gz
  URL_HASH SHA256=b3a24de97a8fdbc835b9833169501030b8977031bcb54b3b3ac13740f846ab30
)

FetchContent_Populate(
  aes_siv
)

# make all the targets available to the rest of the build
FetchContent_MakeAvailable(
  argon2
  openssl
  ed25519
  keccak_tiny
  scrypt
  secp256k1
  murmur3
  gmp
)

# murmur3
add_library(
  murmur3
  STATIC
  ${murmur3_SOURCE_DIR}/murmur3.c
  ${murmur3_SOURCE_DIR}/murmur3.h
)
set(murmur3_SOURCE_DIR ${murmur3_SOURCE_DIR} PARENT_SCOPE)

# openssl
set(openssl_SOURCE_DIR ${openssl_SOURCE_DIR} PARENT_SCOPE)
# file(GLOB_RECURSE openssl_SOURCE_FILES ${openssl_SOURCE_DIR}/*.c ${openssl_SOURCE_DIR}/*.h)
# add_library(
#   openssl
#   STATIC
#   ${openssl_SOURCE_FILES}
# )
# target_include_directories(
#   openssl
#   PUBLIC
#   ${openssl_SOURCE_DIR}
#   ${openssl_SOURCE_DIR}/include
#   ${openssl_SOURCE_DIR}/include/openssl
#   ${openssl_SOURCE_DIR}/include/crypto
#   ${openssl_SOURCE_DIR}/include/internal
# )

# aes_siv
set(aes_siv_SOURCE_DIR ${aes_siv_SOURCE_DIR} PARENT_SCOPE)
file(TOUCH ${aes_siv_SOURCE_DIR}/config.h)
add_library(
  aes_siv
  STATIC
  ${aes_siv_SOURCE_DIR}/aes_siv.c
  ${aes_siv_SOURCE_DIR}/config.h
)
target_include_directories(
  aes_siv
  PUBLIC
  ${openssl_SOURCE_DIR}/include
)

# argon2
set(argon2_SOURCE_DIR ${argon2_SOURCE_DIR} PARENT_SCOPE)
add_library(
  argon2
  STATIC
  ${argon2_SOURCE_DIR}/src/argon2.c
  ${argon2_SOURCE_DIR}/src/blake2/blake2-impl.h
  ${argon2_SOURCE_DIR}/src/blake2/blake2b.c
  ${argon2_SOURCE_DIR}/src/core.c
  ${argon2_SOURCE_DIR}/src/core.h
  ${argon2_SOURCE_DIR}/src/encoding.c
  ${argon2_SOURCE_DIR}/src/encoding.h
  ${argon2_SOURCE_DIR}/src/thread.c
  ${argon2_SOURCE_DIR}/src/thread.h
  ${argon2_SOURCE_DIR}/src/blake2/blamka-round-ref.h
  ${argon2_SOURCE_DIR}/src/ref.c
)
target_include_directories(
  argon2
  PUBLIC
  ${argon2_SOURCE_DIR}/include
  ${argon2_SOURCE_DIR}/src/blake2
)
target_compile_options(
  argon2
  PUBLIC
  -std=c89
  -O3
  -Wall
  -g
  -Isrc
  -DARGON2_NO_THREADS
)

# ed25519
file(GLOB ed25519_SOURCE_FILES ${ed25519_SOURCE_DIR}/src/*.c ${ed25519_SOURCE_DIR}/src/*.h)
list(REMOVE_ITEM ed25519_SOURCE_FILES ${ed25519_SOURCE_DIR}/src/ed25519.h)
add_library(
  ed25519
  STATIC
  ${ed25519_SOURCE_FILES}
)
set(ed25519_SOURCE_DIR ${ed25519_SOURCE_DIR} PARENT_SCOPE)

# keccak_tiny
set(keccak_tiny_SOURCE_DIR ${keccak_tiny_SOURCE_DIR} PARENT_SCOPE)
add_library(
  keccak_tiny
  STATIC
  ${keccak_tiny_SOURCE_DIR}/keccak-tiny.c
  ${keccak_tiny_SOURCE_DIR}/define-macros.h
)
target_include_directories(
  keccak_tiny
  PUBLIC
  ${keccak_tiny_SOURCE_DIR}
)
target_compile_options(
  keccak_tiny
  PUBLIC
  -O3
)

# scrypt
set(scrypt_SOURCE_DIR ${scrypt_SOURCE_DIR} PARENT_SCOPE)
add_library(
  scrypt
  STATIC
  ${scrypt_SOURCE_DIR}/b64.c
  ${scrypt_SOURCE_DIR}/b64.h
  ${scrypt_SOURCE_DIR}/crypto-mcf.c
  ${scrypt_SOURCE_DIR}/crypto-scrypt-saltgen.c
  ${scrypt_SOURCE_DIR}/crypto_scrypt-check.c
  ${scrypt_SOURCE_DIR}/crypto_scrypt-hash.c
  ${scrypt_SOURCE_DIR}/crypto_scrypt-nosse.c
  ${scrypt_SOURCE_DIR}/sha256.c
  ${scrypt_SOURCE_DIR}/sha256.h
  ${scrypt_SOURCE_DIR}/slowequals.c
  ${scrypt_SOURCE_DIR}/slowequals.h
  ${scrypt_SOURCE_DIR}/sysendian.h
)
target_include_directories(
  scrypt
  PUBLIC
  ${scrypt_SOURCE_DIR}
)
target_compile_options(
  scrypt
  PUBLIC
  -O2
  -Wall
  -g
  -fstack-protector
)
target_compile_definitions(
  scrypt
  PUBLIC
  _FORTIFY_SOURCE=2
)

# secp256k1
set(secp256k1_SOURCE_DIR ${secp256k1_SOURCE_DIR} PARENT_SCOPE)

# gmp
file(GLOB gmp_SOURCE_FILES ${gmp_SOURCE_DIR}/*)
add_library(
  gmp
  STATIC
  ${gmp_SOURCE_FILES}
)
set(gmp_SOURCE_DIR ${gmp_SOURCE_DIR} PARENT_SCOPE)