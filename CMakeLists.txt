cmake_minimum_required(VERSION 3.16)

project(MiniText VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.10 libadwaita-1>=1.5)

set(MINITEXT_SOURCES
    src/application.c
    src/editor.c
    src/file_manager.c
    src/line_numbers.c
    src/window.c
)

add_executable(minitext main.c ${MINITEXT_SOURCES})
target_include_directories(minitext PRIVATE src)
target_compile_options(minitext PRIVATE -Wall -Wextra -Werror)
target_link_libraries(minitext PRIVATE PkgConfig::GTK4)

include(CTest)
if(BUILD_TESTING)
    add_executable(test-file-manager
        tests/test_file_manager.c
        src/file_manager.c
    )
    target_include_directories(test-file-manager PRIVATE src)
    target_compile_options(test-file-manager PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(test-file-manager PRIVATE PkgConfig::GTK4)
    add_test(NAME file-manager COMMAND test-file-manager)

    add_executable(test-window-smoke
        tests/test_window_smoke.c
        ${MINITEXT_SOURCES}
    )
    target_include_directories(test-window-smoke PRIVATE src)
    target_compile_options(test-window-smoke PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(test-window-smoke PRIVATE PkgConfig::GTK4)

    add_custom_target(test-ui
        COMMAND ${CMAKE_COMMAND} -E env G_DEBUG=fatal-criticals
                $<TARGET_FILE:test-window-smoke>
        DEPENDS test-window-smoke
        USES_TERMINAL
    )
endif()

include(GNUInstallDirs)

install(
    TARGETS minitext
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/io.github.sametbilir33.MiniText.desktop"
    DESTINATION
        "${CMAKE_INSTALL_DATADIR}/applications"
)

install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/data/metainfo/io.github.sametbilir33.MiniText.metainfo.xml"
    DESTINATION
        "${CMAKE_INSTALL_DATADIR}/metainfo"
)

install(
    DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/data/icons/hicolor/"
    DESTINATION
        "${CMAKE_INSTALL_DATADIR}/icons/hicolor"
)