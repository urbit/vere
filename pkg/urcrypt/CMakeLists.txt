get_filename_component(LIBRARY_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

set(Header_Files "urcrypt.h" "util.h")
source_group("Header Files" FILES "${Header_Files}")

set(Source_Files
        "aes_cbc.c"
        "aes_ecb.c"
        "aes_siv.c"
        "argon.c"
        "ed25519.c"
        "ge_additions.c"
        "keccak.c"
        "ripemd.c"
        "scrypt.c"
        "secp256k1.c"
        "sha.c"
        "util.c"
)
source_group("Source Files" FILES "${Source_Files}")

add_library("${LIBRARY_NAME}" "${Header_Files}" "${Source_Files}")

find_package(unofficial-argon2 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
add_subdirectory("ge-additions")
target_link_libraries(
        "${LIBRARY_NAME}"
        PRIVATE
        "${PROJECT_NAME}_compiler_flags"
        ge-additions
        unofficial::argon2::libargon2
        OpenSSL::SSL OpenSSL::Crypto
)

include(GNUInstallDirs)

target_include_directories(
        "${LIBRARY_NAME}"
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

###########
# Install #
###########

set_property(TARGET "${LIBRARY_NAME}" PROPERTY VERSION "${${PROJECT_NAME}_VERSION}")
set_property(TARGET "${LIBRARY_NAME}" PROPERTY SOVERSION "${${PROJECT_NAME}_VERSION_MAJOR}")

include(GenerateExportHeader)
set(_export_file "${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}_export.h")
generate_export_header("${LIBRARY_NAME}" EXPORT_FILE_NAME "${_export_file}")

install(FILES ${Header_Files} "${_export_file}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
