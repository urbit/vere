get_filename_component(LIBRARY_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

set(Header_Files "ent.h")
source_group("Header Files" FILES "${Header_Files}")

set(Source_Files "ent.c")
source_group("Source Files" FILES "${Source_Files}")

# TODO: `add_test` for tests.c

add_library("${LIBRARY_NAME}" "${Header_Files}" "${Source_Files}")

if (DEFINED ENT_GETENTROPY_BCRYPTGENRANDOM)
    target_compile_definitions("${LIBRARY_NAME}" PRIVATE ENT_GETENTROPY_BCRYPTGENRANDOM=1)
elseif (DEFINED ENT_DEV_URANDOM)
    target_compile_definitions("${LIBRARY_NAME}" PRIVATE ENT_DEV_URANDOM=1)
elseif (DEFINED ENT_GETENTROPY_UNISTD)
    target_compile_definitions("${LIBRARY_NAME}" PRIVATE ENT_GETENTROPY_UNISTD=1)
elseif (DEFINED ENT_GETENTROPY_SYSRANDOM)
    target_compile_definitions("${LIBRARY_NAME}" PRIVATE ENT_GETENTROPY_SYSRANDOM=1)
elseif (DEFINED ENT_GETRANDOM_SYSCALL)
    target_compile_definitions("${LIBRARY_NAME}" PRIVATE ENT_GETRANDOM_SYSCALL=1)
else ()
    message(FATAL_ERROR "One of these must be set: ENT_GETENTROPY_BCRYPTGENRANDOM, ENT_DEV_URANDOM, ENT_GETENTROPY_UNISTD, ENT_GETENTROPY_SYSRANDOM, ENT_GETRANDOM_SYSCALL")
endif ()

include(GNUInstallDirs)

target_link_libraries(
        "${LIBRARY_NAME}"
        PRIVATE
        "${PROJECT_NAME}_compiler_flags"
)

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
