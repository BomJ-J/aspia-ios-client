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

list(APPEND SOURCE_COMMON
    clipboard.cc
    clipboard.h
    clipboard_file_transfer.cc
    clipboard_file_transfer.h
    desktop_session_constants.cc
    desktop_session_constants.h
    file_depacketizer.cc
    file_depacketizer.h
    file_packet.h
    file_packetizer.cc
    file_packetizer.h
    file_platform_util.h
    file_request_handler.cc
    file_request_handler.h
    file_task.cc
    file_task.h
    file_task_factory.cc
    file_task_factory.h
    keycode_converter.cc
    keycode_converter.h
    system_info_constants.cc
    system_info_constants.h)

if (WIN32)
    list(APPEND SOURCE_COMMON
        clipboard_win.cc
        clipboard_win.h
        file_platform_util_win.cc)
endif()

if (LINUX)
    list(APPEND SOURCE_COMMON
        clipboard_x11.cc
        clipboard_x11.h
        file_platform_util_linux.cc)
endif()

if (MOBILE)
    # Plain Qt implementations, shared by Android and iOS.
    list(APPEND SOURCE_COMMON
        clipboard_android.cc
        clipboard_android.h
        file_platform_util_android.cc)
endif()

if (MACOS)
    list(APPEND SOURCE_COMMON
        clipboard_mac.mm
        clipboard_mac.h
        file_platform_util_mac.mm)

    include(mac/sources.cmake)
endif()

list(APPEND SOURCE_COMMON_TESTS
    file_depacketizer_unittest.cc
    file_packetizer_unittest.cc
    tests_main.cc)

include(desktop/sources.cmake)

if (NOT MOBILE)
    include(sys_info/sources.cmake)
endif()

if (WIN32)
    include(win/sources.cmake)
endif()

if (MOBILE)
    include(android/sources.cmake)
endif()

if (IOS)
    # The touch controls are shared with Android; only the log archive talks to the system.
    list(REMOVE_ITEM SOURCE_COMMON_ANDROID ${CMAKE_CURRENT_SOURCE_DIR}/android/log_archiver.cc)
    list(APPEND SOURCE_COMMON_ANDROID ${CMAKE_CURRENT_SOURCE_DIR}/ios/log_archiver_ios.cc)
endif()

list(APPEND SOURCE_COMMON_RESOURCES resources/common.qrc)

source_group("" FILES ${SOURCE_COMMON} ${SOURCE_COMMON_TESTS})
source_group(desktop FILES ${SOURCE_COMMON_DESKTOP})
source_group(sys_info FILES ${SOURCE_COMMON_SYS_INFO})
source_group(resources FILES ${SOURCE_COMMON_RESOURCES})

if (WIN32)
    source_group(win FILES ${SOURCE_COMMON_WIN})
endif()

if (MACOS)
    source_group(mac FILES ${SOURCE_COMMON_MAC})
endif()

if (MOBILE)
    source_group(android FILES ${SOURCE_COMMON_ANDROID})
endif()

add_library(aspia_common STATIC
    ${SOURCE_COMMON}
    ${SOURCE_COMMON_ANDROID}
    ${SOURCE_COMMON_DESKTOP}
    ${SOURCE_COMMON_SYS_INFO}
    ${SOURCE_COMMON_WIN}
    ${SOURCE_COMMON_MAC}
    ${SOURCE_COMMON_RESOURCES})
target_link_libraries(aspia_common PRIVATE
    aspia_base
    aspia_proto
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})
set_property(TARGET aspia_common PROPERTY AUTOMOC ON)
set_property(TARGET aspia_common PROPERTY AUTORCC ON)

add_ui_headers(aspia_common)

if (WIN32)
    set(COMMON_TESTS_PLATFORM_LIBS avrt crypt32 dbghelp iphlpapi ws2_32)
endif()

if (LINUX)
    set(COMMON_TESTS_PLATFORM_LIBS stdc++fs)
endif()

if (MACOS)
    set(COMMON_TESTS_PLATFORM_LIBS ${FOUNDATION_LIB})
endif()

if (IOS)
    # Unit tests are run on the desktop systems; an iOS executable has to be an application bundle.
    return()
endif()

add_executable(aspia_common_tests ${SOURCE_COMMON_TESTS})
target_link_libraries(aspia_common_tests PRIVATE
    aspia_common
    aspia_base
    aspia_proto
    GTest::gtest
    Qt6::Test
    ${COMMON_TESTS_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS})
set_property(TARGET aspia_common_tests PROPERTY AUTOMOC ON)
set_property(TARGET aspia_common_tests PROPERTY AUTOUIC ON)
set_property(TARGET aspia_common_tests PROPERTY AUTORCC ON)
qt_import_plugins(aspia_common_tests INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE "" EXCLUDE_BY_TYPE imageformats)

add_test(NAME aspia_common_tests COMMAND aspia_common_tests)
