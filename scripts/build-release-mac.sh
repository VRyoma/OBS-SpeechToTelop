#!/usr/bin/env bash
# Local release build for macOS (arm64).
# Works with Command Line Tools only (no full Xcode required).
# Requires: brew install ninja
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OBS_VERSION="30.1.2"
CMAKE3_VERSION="3.31.6"
OBS_DIR="$ROOT/.obs-studio"
OBS_BUILD="$ROOT/.obs-build"
OBS_INSTALL="$ROOT/.obs-install"
BUILD_DIR="$ROOT/build-release"
CMAKE3_DIR="$ROOT/.cmake3"

# ── 1. cmake 3.x ─────────────────────────────────────────────────────────────
# obs-studio 30.x cmake scripts are incompatible with cmake 4.x.
# Download cmake 3.x binary if cmake in PATH is 4.x.
CMAKE_BIN="cmake"
cmake_major() { "$1" --version 2>/dev/null | head -1 | sed 's/[^0-9]*\([0-9]*\).*/\1/'; }

if [ "$(cmake_major cmake 2>/dev/null)" -ge 4 ] 2>/dev/null; then
    CMAKE3_BIN="$CMAKE3_DIR/CMake.app/Contents/bin/cmake"
    if [ ! -x "$CMAKE3_BIN" ]; then
        echo "Downloading cmake $CMAKE3_VERSION..."
        TGZ="cmake-${CMAKE3_VERSION}-macos-universal.tar.gz"
        URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE3_VERSION}/${TGZ}"
        curl -fsSL "$URL" | tar -xz -C "$ROOT"
        mv "$ROOT/cmake-${CMAKE3_VERSION}-macos-universal" "$CMAKE3_DIR"
        echo "cmake $CMAKE3_VERSION downloaded to $CMAKE3_DIR"
    fi
    CMAKE_BIN="$CMAKE3_BIN"
fi
echo "Using cmake: $CMAKE_BIN ($("$CMAKE_BIN" --version | head -1))"

# ── 2. Ninja check ────────────────────────────────────────────────────────────
if ! command -v ninja &>/dev/null; then
    echo "ninja not found. Install with: brew install ninja"
    exit 1
fi

# ── 3. Clone / update obs-studio ─────────────────────────────────────────────
if [ ! -d "$OBS_DIR/.git" ]; then
    git clone --depth 1 --branch "$OBS_VERSION" \
        https://github.com/obsproject/obs-studio.git "$OBS_DIR"
else
    echo "obs-studio already cloned at $OBS_DIR"
fi

# ── 4. Patch obs-studio ───────────────────────────────────────────────────────
# 4a. Remove Xcode-generator requirement (compilerconfig.cmake)
COMPILER_CFG="$OBS_DIR/cmake/macos/compilerconfig.cmake"
if [ -f "$COMPILER_CFG" ] && grep -q "requires Xcode generator" "$COMPILER_CFG"; then
    sed -i '' \
        's/message(FATAL_ERROR "Building OBS Studio on macOS requires Xcode generator\.")/message(STATUS "Xcode generator not required (patched for CLT build)")/' \
        "$COMPILER_CFG"
    echo "Patched: removed Xcode generator requirement"
fi

# 4b. Remove media-playback (needs FFmpeg, not required by libobs)
DEPS_CMAKE="$OBS_DIR/deps/CMakeLists.txt"
if [ -f "$DEPS_CMAKE" ] && grep -q "add_subdirectory(media-playback)" "$DEPS_CMAKE"; then
    sed -i '' \
        's/add_subdirectory(media-playback)/# add_subdirectory(media-playback)/' \
        "$DEPS_CMAKE"
    echo "Patched: disabled media-playback"
fi

# ── 5. Build libobs ───────────────────────────────────────────────────────────
if [ ! -f "$OBS_INSTALL/lib/cmake/libobs/libobsConfig.cmake" ]; then
    "$CMAKE_BIN" -B "$OBS_BUILD" -S "$OBS_DIR" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DENABLE_BROWSER=OFF \
        -DENABLE_WEBSOCKET=OFF \
        -DENABLE_UI=OFF \
        -DENABLE_SCRIPTING=OFF \
        -DENABLE_VLC=OFF \
        -DENABLE_PLUGINS=OFF \
        -DENABLE_FRONTEND_API=OFF \
        -DENABLE_HEVC=OFF \
        -DENABLE_VIRTUALCAM=OFF

    "$CMAKE_BIN" --build "$OBS_BUILD" --target libobs --parallel
    "$CMAKE_BIN" --install "$OBS_BUILD" --prefix "$OBS_INSTALL"
    echo "libobs built and installed to $OBS_INSTALL"
else
    echo "libobs already installed at $OBS_INSTALL (delete to rebuild)"
fi

# Find libobsConfig.cmake
LIBOBS_DIR="$(find "$OBS_INSTALL" "$OBS_BUILD" -name "libobsConfig.cmake" 2>/dev/null | head -1 | xargs -I{} dirname {})"
if [ -z "$LIBOBS_DIR" ]; then
    echo "Error: libobsConfig.cmake not found"
    exit 1
fi

# ── 6. Build plugin ───────────────────────────────────────────────────────────
GTEST_ROOT="$(brew --prefix googletest 2>/dev/null || true)"

"$CMAKE_BIN" -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -Dlibobs_DIR="$LIBOBS_DIR" \
    ${GTEST_ROOT:+-DGTEST_ROOT="$GTEST_ROOT"}

"$CMAKE_BIN" --build "$BUILD_DIR" --parallel

echo ""
echo "Build succeeded!"
echo "Plugin: $BUILD_DIR/SpeechToTelop.dylib"
echo ""
echo "Install to OBS:"
echo "  mkdir -p ~/Library/Application\\ Support/obs-studio/plugins/SpeechToTelop/bin"
echo "  cp $BUILD_DIR/SpeechToTelop.dylib ~/Library/Application\\ Support/obs-studio/plugins/SpeechToTelop/bin/"
