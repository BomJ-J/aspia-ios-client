#
# Aspia Project
# Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

cmake_minimum_required(VERSION 3.30.0)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(aspia)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cmake)

include(Utils)

if (WIN32)
    include(FindWDK)
endif()

set(ASPIA_VERSION_MAJOR 3)
set(ASPIA_VERSION_MINOR 1)
set(ASPIA_VERSION_PATCH 2)

# On CI installed dir could be outside of vcpkg root
if ("$ENV{VCPKG_INSTALLED_DIR}" STREQUAL "")
    set(VCPKG_INSTALLED_DIR $ENV{VCPKG_ROOT}/installed)
    message(STATUS "VCPKG_INSTALLED_DIR -> redefine: ${VCPKG_INSTALLED_DIR}")
else()
    set(VCPKG_INSTALLED_DIR $ENV{VCPKG_INSTALLED_DIR})
    message(STATUS "VCPKG_INSTALLED_DIR -> external: ${VCPKG_INSTALLED_DIR}")
endif()

message(STATUS "VCPKG_TARGET_TRIPLET: ${VCPKG_TARGET_TRIPLET}")
message(STATUS "VCPKG_HOST_TRIPLET: ${VCPKG_HOST_TRIPLET}")

add_definitions(-DASPIA_VERSION_MAJOR=${ASPIA_VERSION_MAJOR}
                -DASPIA_VERSION_MINOR=${ASPIA_VERSION_MINOR}
                -DASPIA_VERSION_PATCH=${ASPIA_VERSION_PATCH})

# CMake sets IOS for the iOS toolchain since 3.14; set it here as well for older toolchain files.
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IOS TRUE)
endif()

# APPLE is set for both macOS and iOS. MACOS marks the desktop system only.
if (APPLE AND NOT IOS)
    set(MACOS TRUE)
endif()

# Android and iOS share the touch interface of the client (source/client/android).
if (ANDROID OR IOS)
    set(MOBILE TRUE)
    add_definitions(-DASPIA_MOBILE)
endif()

if (IOS)
    message(STATUS "iOS detected")

    # The IPA is handed out to other people: keep the absolute paths of the build machine (with the
    # name of its user) out of __FILE__ and the debug information.
    add_compile_options("-ffile-prefix-map=${CMAKE_SOURCE_DIR}/=" "-ffile-prefix-map=${CMAKE_BINARY_DIR}/=")
endif()

if (ANDROID)
    message(STATUS "Android detected")

    # Only the Android toolchain knows the NDK sysroot, so generate the clangd config here. This lets
    # clangd resolve <jni.h> for the Android sources without committing a machine-specific path.
    configure_file(${CMAKE_SOURCE_DIR}/.clangd.in ${CMAKE_SOURCE_DIR}/.clangd @ONLY)
elseif (UNIX AND NOT APPLE)
    message(STATUS "Linux detected")
    set(LINUX TRUE)
endif()

if (MACOS)
    message(STATUS "MacOSX detected")
endif()

if (APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

if (WIN32)
    message(STATUS "Windows detected")
endif()

set(CMAKE_SKIP_BUILD_RPATH ON)

find_package(Qt6 CONFIG REQUIRED Core Gui Network Test Widgets Xml LinguistTools Svg)

if (NOT IOS)
    # Qt for iOS has neither printing nor the Linux framebuffer support modules.
    find_package(Qt6 CONFIG REQUIRED PrintSupport)
    find_package(Qt6DeviceDiscoverySupportPrivate CONFIG REQUIRED)
    find_package(Qt6FbSupportPrivate CONFIG REQUIRED)
endif()

find_package(asio CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(libyuv CONFIG REQUIRED)
find_package(miniupnpc CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(unofficial-libvpx CONFIG REQUIRED)
find_package(unofficial-libwebm CONFIG REQUIRED)
find_package(unofficial-enet CONFIG REQUIRED)
find_package(unofficial-nayuki-qr-code-generator CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

if (WIN32)
    find_package(Qt6EntryPointPrivate CONFIG REQUIRED)
    find_package(WDK REQUIRED)

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (ANDROID)
    # The oboe vcpkg port installs only the library and headers (no CMake config), so wrap them in an
    # imported target. Oboe pulls in the NDK system libraries log and OpenSLES.
    find_library(OBOE_LIBRARY NAMES oboe REQUIRED)
    find_path(OBOE_INCLUDE_DIR "oboe/Oboe.h" REQUIRED)

    add_library(oboe::oboe UNKNOWN IMPORTED)
    set_target_properties(oboe::oboe PROPERTIES
        IMPORTED_LOCATION "${OBOE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OBOE_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "log;OpenSLES")
endif()

if (LINUX)
    find_package(Qt6XcbQpaPrivate CONFIG REQUIRED)
    find_package(Qt6DBus CONFIG REQUIRED)
    find_package(ICU REQUIRED COMPONENTS uc)
endif()

if (MACOS)
    if (${VCPKG_TARGET_TRIPLET} STREQUAL "arm64-osx")
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    else()
        set(CMAKE_OSX_ARCHITECTURES "x86_64")
    endif()

    message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")

    # ScreenCaptureKit (host screen capture) requires macOS 12.3; 13.0 is the supported floor.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")
    message(STATUS "CMAKE_OSX_DEPLOYMENT_TARGET: ${CMAKE_OSX_DEPLOYMENT_TARGET}")

    find_package(Qt6OpenGL CONFIG REQUIRED)
    find_package(ICU REQUIRED COMPONENTS uc)

    find_library(FOUNDATION_LIB Foundation)
    find_library(COREAUDIO_LIB CoreAudio)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox)
    find_library(SCREENCAPTUREKIT_LIB ScreenCaptureKit)
    find_library(COREMEDIA_LIB CoreMedia)
    find_library(COREVIDEO_LIB CoreVideo)
    find_library(COREGRAPHICS_LIB CoreGraphics)
    find_library(SYSTEMCONFIGURATION_LIB SystemConfiguration)
    find_library(IOKIT_LIB IOKit)
    find_library(METAL_LIB Metal)
    find_library(SECURITY_LIB Security)
    find_library(CARBON_LIB Carbon)
    find_library(APPLICATIONSERVICES_LIB ApplicationServices)
    find_library(APPKIT_LIB AppKit)
    find_library(OSLOG_LIB OSLog)
    find_program(CODESIGN_BIN NAMES codesign)
endif()

if (IOS)
    message(STATUS "CMAKE_OSX_DEPLOYMENT_TARGET: ${CMAKE_OSX_DEPLOYMENT_TARGET}")

    find_library(FOUNDATION_LIB Foundation)
    find_library(UIKIT_LIB UIKit)
    find_library(AVFOUNDATION_LIB AVFoundation)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox)
    find_library(COREMEDIA_LIB CoreMedia)
    find_library(COREVIDEO_LIB CoreVideo)
    find_library(VIDEOTOOLBOX_LIB VideoToolbox)
    find_library(SECURITY_LIB Security)
    find_library(LOCALAUTHENTICATION_LIB LocalAuthentication)
    find_library(SYSTEMCONFIGURATION_LIB SystemConfiguration)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# libwebm headers cross-include via "common/webmids.h", so we need
# <prefix>/include/webm on the search path in addition to <prefix>/include.
get_target_property(_libwebm_inc_dirs unofficial::libwebm::libwebm INTERFACE_INCLUDE_DIRECTORIES)
set(_libwebm_extra_includes)
foreach(_dir IN LISTS _libwebm_inc_dirs)
    list(APPEND _libwebm_extra_includes "${_dir}/webm")
endforeach()
set_property(TARGET unofficial::libwebm::libwebm APPEND PROPERTY
             INTERFACE_INCLUDE_DIRECTORIES ${_libwebm_extra_includes})

enable_testing()
add_subdirectory(source)

file(GLOB TS_FILES_LIST "source/translations/*.ts")
get_target_property(LUPDATE_PATH Qt::lupdate IMPORTED_LOCATION)
set(SOURCE_FILES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_custom_target(update_translations
    COMMAND ${LUPDATE_PATH} -I ${SOURCE_FILES_DIR} -extensions ui,cc,cpp,h ${SOURCE_FILES_DIR} -ts ${TS_FILES_LIST}
    WORKING_DIRECTORY "${SOURCE_FILES_DIR}"
    SOURCES ${TS_FILES_LIST}
    VERBATIM
    COMMAND_EXPAND_LISTS)

qt_add_translation(QM_FILES_LIST ${TS_FILES_LIST})
qt_add_resources(aspia_common "common_translations" PREFIX "tr" BASE "${CMAKE_CURRENT_BINARY_DIR}" FILES "${QM_FILES_LIST}")

if (CMAKE_GENERATOR STREQUAL "Xcode")
    # The new build system of Xcode refuses a custom command attached to several targets unless one
    # of them comes before all the others. The .qm files are made by a target of their own that the
    # targets embedding or listing them depend on. The listing target is created at the end of the
    # directory, so the dependencies are added then.
    add_custom_target(aspia_translations DEPENDS ${QM_FILES_LIST})

    function(aspia_order_translations)
        foreach(target aspia_common aspia_common_common_translations aspia_common_other_files)
            if (TARGET ${target})
                add_dependencies(${target} aspia_translations)
            endif()
        endforeach()
    endfunction()

    cmake_language(DEFER CALL aspia_order_translations)
endif()

message(STATUS "Source files: ${SOURCE_FILES_DIR}")
message(STATUS "TS files: ${TS_FILES_LIST}")
message(STATUS "QM files: ${QM_FILES_LIST}")
