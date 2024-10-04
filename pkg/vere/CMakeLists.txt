get_filename_component(LIBRARY_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

set(Header_Files "mars.h" "serf.h" "vere.h")
source_group("Header Files" FILES "${Header_Files}")

set(Source_Files #"ames_tests.c"
        "auto.c"
        "benchmarks.c"
        #"boot_tests.c"
        "dawn.c"
        "disk.c"
        "foil.c"
        "king.c"
        "lord.c"
        "main.c"
        "mars.c"
        "newt.c"
        #"newt_tests.c"
        #"noun_tests.c"
        "pier.c"
        "save.c"
        "serf.c"
        "time.c"
        #"unix_tests.c"
        "ward.c"

        "io/ames.c"
        "io/behn.c"
        "io/conn.c"
        "io/cttp.c"
        "io/fore.c"
        "io/hind.c"
        "io/http.c"
        "io/lick.c"
        "io/term.c"
        "io/unix.c"
)
source_group("Source Files" FILES "${Source_Files}")

add_library("${LIBRARY_NAME}" "${Header_Files}" "${Source_Files}")

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
