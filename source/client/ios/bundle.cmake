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

# Application bundle settings of the iOS client. Included from client/CMakeLists.txt.

# The identifier has to be unique for the Apple ID that signs the application. With a free account
# Xcode or a sideloading tool may ask for another one; it is changed here or with -D.
set(ASPIA_IOS_BUNDLE_ID "org.aspia.client.ios" CACHE STRING "Bundle identifier of the iOS client")
set(ASPIA_IOS_TEAM_ID "" CACHE STRING "Apple development team used to sign the iOS client")

set(ASPIA_IOS_VERSION "${ASPIA_VERSION_MAJOR}.${ASPIA_VERSION_MINOR}.${ASPIA_VERSION_PATCH}")
if (GIT_COMMIT_COUNT)
    set(ASPIA_IOS_BUILD "${GIT_COMMIT_COUNT}")
else()
    set(ASPIA_IOS_BUILD "${ASPIA_VERSION_PATCH}")
endif()

set_target_properties(aspia_client PROPERTIES
    OUTPUT_NAME "Aspia"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "Aspia"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${ASPIA_IOS_BUNDLE_ID}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${ASPIA_IOS_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${ASPIA_IOS_BUILD}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ASPIA_IOS_BUNDLE_ID}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")

if (ASPIA_IOS_TEAM_ID)
    set_target_properties(aspia_client PROPERTIES
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${ASPIA_IOS_TEAM_ID}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic")
endif()

# The application icon. The asset catalog is compiled by Xcode into the bundle.
target_sources(aspia_client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets" PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
