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
