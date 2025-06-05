#!/bin/bash

# Clipchamp BabylonNative Tests Build Script
# This script builds and runs the Clipchamp-specific BabylonNative tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BABYLON_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="$BABYLON_ROOT/Build/ClipchampTests"

echo "🏗️  Building Clipchamp BabylonNative Tests"
echo "Babylon Root: $BABYLON_ROOT"
echo "Build Directory: $BUILD_DIR"

# Clean build directory
if [ -d "$BUILD_DIR" ]; then
    echo "🧹 Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Build for iOS Simulator (easier for testing)
echo "📱 Configuring for iOS Simulator..."
cmake "$BABYLON_ROOT" \
    -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_OSX_SYSROOT=iphonesimulator \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCLIPCHAMP_TESTS=ON

echo "🔨 Building the project..."
xcodebuild \
    -project BabylonNative.xcodeproj \
    -scheme ALL_BUILD \
    -configuration Debug \
    -sdk iphonesimulator \
    -destination "platform=iOS Simulator,name=iPhone 15,OS=latest" \
    build

echo "✅ Build completed successfully!"

# Check if we can find any test executables
echo "🔍 Looking for test executables..."
find "$BUILD_DIR" -name "*Test*" -type f -executable || echo "No test executables found yet"

echo ""
echo "🎉 Clipchamp BabylonNative Tests build process completed!"
echo ""
echo "Next steps:"
echo "1. The tests are built as part of the BabylonNative project"
echo "2. To run tests manually, you can open the Xcode project at:"
echo "   $BUILD_DIR/BabylonNative.xcodeproj"
echo "3. Look for ClipchampTests targets in the project"
echo ""
echo "Note: Some tests may require actual device/simulator execution"
echo "      due to Metal and iOS-specific dependencies."
