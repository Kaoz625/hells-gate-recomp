#!/bin/sh
# ios/build-ios.sh <generated.tgz from a desktop codegen> - Dante's Inferno for iPhone/iPad (blz/ios, 17 Nov 2026).
# 1. SDK: rexglue-sdk v0.10.0 + patches/sdk/rexglue-sdk-v0.10.0.patch (Hell's Gate) + ios/sdk-patches/*.patch
#    (0003 = DevZer0D4Y/MCX360-EDITION sdk/rexglue-sdk-ios.patch, BSD-3, merged; 0004 = iOS stereo; 0005 = iOS user
#    folder), then `cmake --preset ios-arm64 && cmake --build out/build/ios-arm64 --config Release && cmake --install`.
# 2. Codegen on a desktop host (the iOS SDK has no rexglue tool) with the SAME Hell's Gate-patched SDK, from your own
#    default.xex + default.xexp (TU2, Media ID 27C36C6C package), then patches/generated/apply_generated_patches.py;
#    copy generated/ here.
# 3. This script: configure + build the app bundle. Sign with an App ID that has extended-virtual-addressing +
#    increased-memory-limit, install, copy the extracted disc + default.xexp into the app's Documents/game.
set -e
SDK_INSTALL="${SDK_INSTALL:-$HOME/src/dante-ios/rexglue-sdk/out/install/ios-arm64}"
BUNDLE_ID="${BUNDLE_ID:-com.nyct.x360.dantesinferno.MNU46BX5M3}"
cd "$(dirname "$0")/.."
[ -n "$1" ] && tar -xzf "$1"
cmake -S . -B out/build/ios-arm64-release -G Ninja \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  "-DCMAKE_C_FLAGS=-march=armv8-a" "-DCMAKE_CXX_FLAGS=-march=armv8-a" \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DCMAKE_PREFIX_PATH="$SDK_INSTALL" \
  -Drexglue_DIR="$SDK_INSTALL/lib/cmake/rexglue" -DDANTESINFERNO_BUNDLE_ID="$BUNDLE_ID"
cmake --build out/build/ios-arm64-release -j 8
echo "app: out/build/ios-arm64-release/dantes_inferno.app"
