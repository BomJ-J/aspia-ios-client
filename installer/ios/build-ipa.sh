#!/bin/zsh
# Builds an unsigned Aspia IPA on a Mac: builds/Aspia-unsigned.ipa.
#
#   installer/ios/build-ipa.sh [bundle id]
#
# The first run builds the vcpkg dependencies (up to 1-2 hours); later runs rebuild only what
# changed. Qt is taken from QT_IOS_PATH and QT_HOST_PATH, or from ~/Qt/<QT_VERSION> by default.
set -e
set -o pipefail

QT_VERSION=${QT_VERSION:-6.11.1}
BUNDLE_ID=${1:-${ASPIA_IOS_BUNDLE_ID:-}}

ROOT=${0:A:h:h:h}
cd "$ROOT"

[ -x /opt/homebrew/bin/brew ] && eval "$(/opt/homebrew/bin/brew shellenv)"
export QT_IOS_PATH=${QT_IOS_PATH:-$HOME/Qt/$QT_VERSION/ios}
export QT_HOST_PATH=${QT_HOST_PATH:-$HOME/Qt/$QT_VERSION/macos}
# xcode-select may point to the Command Line Tools, which have no iOS SDK.
if ! xcrun --sdk iphoneos --show-sdk-path >/dev/null 2>&1; then
  export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi

if [ ! -f "$QT_IOS_PATH/lib/cmake/Qt6/qt.toolchain.cmake" ] || [ ! -x "$QT_HOST_PATH/libexec/moc" ]; then
  echo "Qt $QT_VERSION for iOS and macOS not found in $QT_IOS_PATH and $QT_HOST_PATH" >&2
  exit 1
fi
[ -x vcpkg4aspia/vcpkg ] || ./vcpkg4aspia/bootstrap-vcpkg.sh -disableMetrics

mkdir -p builds
# Configure on every run: when CMake regenerates the project from inside the Xcode build instead,
# vcpkg sees the Xcode build environment and rebuilds every dependency, which fails for libvpx.
# With nothing changed this takes seconds.
echo "==> Configure"
if ! cmake --preset local-ios ${BUNDLE_ID:+-DASPIA_IOS_BUNDLE_ID=$BUNDLE_ID} > builds/configure.log 2>&1; then
  tail -n 30 builds/configure.log
  echo "Configure failed, full log: builds/configure.log" >&2
  exit 1
fi

echo "==> Build"
if ! cmake --build --preset local-ios-app -- -sdk iphoneos \
    CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" > builds/build.log 2>&1; then
  grep -E "error:" builds/build.log | head -n 30
  tail -n 20 builds/build.log
  echo "Build failed, full log: builds/build.log" >&2
  exit 1
fi

echo "==> Package"
APP=$(find builds/local-ios -type d -name "Aspia.app" -not -path "*.dSYM*" -not -path "*/build/*" | head -n 1)
[ -n "$APP" ] || { echo "Aspia.app not found" >&2; exit 1; }
rm -rf builds/ipa
mkdir -p builds/ipa/Payload
cp -R "$APP" builds/ipa/Payload/
rm -f builds/Aspia-unsigned.ipa
(cd builds/ipa && zip -qry ../Aspia-unsigned.ipa Payload)

echo "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' builds/ipa/Payload/Aspia.app/Info.plist)" \
  "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' builds/ipa/Payload/Aspia.app/Info.plist)"
ls -lh builds/Aspia-unsigned.ipa
