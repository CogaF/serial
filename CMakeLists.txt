cmake_minimum_required(VERSION 3.15)
project(serial VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Commit count for the version suffix
# -----------------------------------------------------------------------------
find_package(Git QUIET)
set(SERIAL_COMMIT_COUNT 0)
if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE SERIAL_COMMIT_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT SERIAL_COMMIT_COUNT MATCHES "^[0-9]+$")
        set(SERIAL_COMMIT_COUNT 0)
    endif()
endif()
set(SERIAL_VERSION_STRING "${PROJECT_VERSION}+${SERIAL_COMMIT_COUNT}")

# -----------------------------------------------------------------------------
# Published artifact layout: mirror the Visual Studio layout under
#   <source>/serial_lib_structure/<arch>/<Config>/<lib|dll>/
# so downstream projects (peltier_cntl_tec_8a and others) can resolve
# serial.lib / serial.dll by the same scheme on every platform.
#   - <arch> is x86 or x64 (never Win32).
#   - <Config> is Debug or Release.
#   - <lib|dll> is lib for a static build, dll for a shared build.
# Headers are copied under serial_lib_structure/include/serial/ as a
# post-build step on the target below.
# -----------------------------------------------------------------------------
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SERIAL_PLATFORM_TAG "x64")
else()
    set(SERIAL_PLATFORM_TAG "x86")
endif()
if(BUILD_SHARED_LIBS)
    set(SERIAL_LINK_KIND "dll")
else()
    set(SERIAL_LINK_KIND "lib")
endif()
set(SERIAL_STRUCTURE_ROOT "${CMAKE_SOURCE_DIR}/serial_lib_structure")
if(CMAKE_CONFIGURATION_TYPES)
    foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${_cfg}" _CFG)
        if(_cfg MATCHES "^[Dd]ebug")
            set(_dep_cfg "Debug")
        else()
            set(_dep_cfg "Release")
        endif()
        set(_out "${SERIAL_STRUCTURE_ROOT}/${SERIAL_PLATFORM_TAG}/${_dep_cfg}/${SERIAL_LINK_KIND}")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_CFG} "${_out}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_CFG} "${_out}")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_CFG} "${_out}")
    endforeach()
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
    endif()
    if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
        set(_dep_cfg "Debug")
    else()
        set(_dep_cfg "Release")
    endif()
    set(_out "${SERIAL_STRUCTURE_ROOT}/${SERIAL_PLATFORM_TAG}/${_dep_cfg}/${SERIAL_LINK_KIND}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${_out}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${_out}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_out}")
endif()

# -----------------------------------------------------------------------------
# Catkin shim (ROS 1) — only engaged when invoked from a catkin workspace.
# -----------------------------------------------------------------------------
if(CATKIN_DEVEL_PREFIX OR CATKIN_BUILD_BINARY_PACKAGE)
    find_package(catkin REQUIRED)
    if(UNIX AND NOT APPLE)
        catkin_package(LIBRARIES ${PROJECT_NAME} INCLUDE_DIRS include DEPENDS rt pthread)
    else()
        catkin_package(LIBRARIES ${PROJECT_NAME} INCLUDE_DIRS include)
    endif()
endif()

# -----------------------------------------------------------------------------
# Platform discovery
# -----------------------------------------------------------------------------
set(SERIAL_HAVE_LIBUDEV FALSE)
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBUDEV libudev)
        if(LIBUDEV_FOUND)
            set(SERIAL_HAVE_LIBUDEV TRUE)
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(serial_SRCS
    src/serial.cc
    src/impl/hotplug.cc
)
if(APPLE)
    list(APPEND serial_SRCS
        src/impl/unix.cc
        src/impl/list_ports/list_ports_osx.cc
        src/impl/hotplug_osx.cc)
elseif(UNIX)
    list(APPEND serial_SRCS
        src/impl/unix.cc
        src/impl/list_ports/list_ports_linux.cc
        src/impl/hotplug_linux.cc)
else()
    list(APPEND serial_SRCS
        src/impl/win.cc
        src/impl/list_ports/list_ports_win.cc
        src/impl/hotplug_win.cc)
endif()

add_library(${PROJECT_NAME} ${serial_SRCS})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SERIAL_COMMIT_COUNT=${SERIAL_COMMIT_COUNT}
    SERIAL_VERSION_STRING="${SERIAL_VERSION_STRING}"
)
# When built shared on Windows, mark public symbols as exported.
# Consumers must define SERIAL_USE_DLL when linking against the DLL.
if(WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SERIAL_BUILD_DLL)
    if(MSVC)
        # C4251/C4275: STL types in dll-exported classes. Benign when the
        # DLL and its consumers are built with the same compiler/runtime,
        # which is the case for the configurations we ship.
        target_compile_options(${PROJECT_NAME} PRIVATE /wd4251 /wd4275)
    endif()
endif()
if(SERIAL_HAVE_LIBUDEV)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SERIAL_HAVE_LIBUDEV)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUDEV_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUDEV_LIBRARIES})
endif()
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(APPLE)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(FOUNDATION_LIBRARY Foundation)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${FOUNDATION_LIBRARY} ${IOKIT_LIBRARY})
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread rt dl)
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC setupapi)
endif()

# Publish public headers into serial_lib_structure/include/serial after each
# successful build, so the artifact tree mirrors the Visual Studio layout.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${SERIAL_STRUCTURE_ROOT}/include/serial/impl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/include/serial/serial.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/serial/v8stdint.h"
        "${SERIAL_STRUCTURE_ROOT}/include/serial/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/include/serial/impl/unix.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/serial/impl/win.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/serial/impl/win_compat.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/serial/impl/hotplug.h"
        "${SERIAL_STRUCTURE_ROOT}/include/serial/impl/"
    COMMENT "Publishing headers to serial_lib_structure/include/"
    VERBATIM)

# Example
add_executable(serial_example examples/serial_example.cc)
target_link_libraries(serial_example PRIVATE ${PROJECT_NAME})

# -----------------------------------------------------------------------------
# Install / export
# -----------------------------------------------------------------------------
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

if(CATKIN_DEVEL_PREFIX OR CATKIN_BUILD_BINARY_PACKAGE)
    install(TARGETS ${PROJECT_NAME}
        ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
        LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
        RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION})
    install(FILES include/serial/serial.h include/serial/v8stdint.h
        DESTINATION ${CATKIN_GLOBAL_INCLUDE_DESTINATION}/serial)
else()
    install(TARGETS ${PROJECT_NAME}
        EXPORT serialTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT serialTargets
        FILE serialTargets.cmake
        NAMESPACE serial::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serial)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/serialConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/serialConfig.cmake"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/serialTargets.cmake\")\n")
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/serialConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/serialConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serial)
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
if(BUILD_TESTING OR CATKIN_ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
