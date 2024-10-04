get_filename_component(LIBRARY_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

set(Header_Files "c3.h" "defs.h" "motes.h" "portable.h" "types.h")
source_group("Header Files" FILES "${Header_Files}")

set(Source_Files "defs.c")
source_group("Source Files" FILES "${Source_Files}")

add_library("${LIBRARY_NAME}" "${Header_Files}" "${Source_Files}")

if (LINUX)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC -DU3_OS_linux=1)
elseif (APPLE)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC -DU3_OS_osx=1)
elseif (BSD)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC -DU3_OS_bsd=1)
else ()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} unsupported. Port it!")
endif ()

if (CMAKE_VERSION VERSION_LESS "3.20")
    include(TestBigEndian)
    test_big_endian(CMAKE_C_BYTE_ORDER)
endif ()
if (CMAKE_C_BYTE_ORDER EQUAL 1)
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC -DU3_OS_ENDIAN_big=1)
else ()
    target_compile_definitions("${LIBRARY_NAME}" PUBLIC -DU3_OS_ENDIAN_little=1)
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
