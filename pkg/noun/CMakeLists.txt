get_filename_component(LIBRARY_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

set(Header_Files "allocate.h"
        "error.h"
        "events.h"
        "hashtable.h"
        "imprison.h"
        "jets.h"
        "log.h"
        "manage.h"
        "nock.h"
        "noun.h"
        "options.h"
        "retrieve.h"
        "serial.h"
        "trace.h"
        "types.h"
        "urth.h"
        "version.h"
        "vortex.h"
        "xtract.h"
        "zave.h"
)
source_group("Header Files" FILES "${Header_Files}")

set(Source_Files
        "allocate.c"
        "events.c"
        "hashtable.c"
        # "hashtable_tests.c"
        "imprison.c"
        "jets.c"
        # "jets_tests.c"
        "log.c"
        "manage.c"
        "nock.c"
        # "nock_tests.c"
        "options.c"
        "retrieve.c"
        # "retrieve_tests.c"
        "serial.c"
        # "serial_tests.c"
        "trace.c"
        "urth.c"
        "vortex.c"
        "xtract.c"
        "zave.c")
source_group("Source Files" FILES "${Source_Files}")

add_library("${LIBRARY_NAME}" "${Header_Files}" "${Source_Files}")

include(GNUInstallDirs)

target_link_libraries(
        "${LIBRARY_NAME}"
        PRIVATE
        "${PROJECT_NAME}_compiler_flags"
)

set(rsignal "${CMAKE_CURRENT_SOURCE_DIR}/platform")
if (LINUX)
    set(rsignal "${rsignal}/linux")
elseif (APPLE)
    set(rsignal "${rsignal}/darwin")
else ()
    message(FATAL_ERROR "You need to port rsignal.h")
endif ()
target_include_directories(
        "${LIBRARY_NAME}"
        PUBLIC
        "$<BUILD_INTERFACE:${rsignal}>"
)
set(rsignal "${rsignal}/rsignal.h")


target_link_libraries(
        "${LIBRARY_NAME}"
        PUBLIC
        "c3"
        "ur"
)

target_include_directories(
        "${LIBRARY_NAME}"
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pkg>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_include_directories(
        "${LIBRARY_NAME}"
        PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# TODO: Port libavutil to vcpkg
find_path(LIBAVUTIL_INCLUDE_DIR
        NAMES libavutil/murmur3.h
        PATHS
        /usr/include /usr/local/include    #system level
        /opt/local/include /sw/include     #macports & fink
        /usr/include/x86_64-linux-gnu
        /usr/include/x86_64-linux-gnu/libavutil
        PATH_SUFFIXES libav ffmpeg libavutil
        REQUIRED
)
target_include_directories(
        "${LIBRARY_NAME}"
        PRIVATE
        "${LIBAVUTIL_INCLUDE_DIR}"
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
install(FILES ${rsignal}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/platform")
