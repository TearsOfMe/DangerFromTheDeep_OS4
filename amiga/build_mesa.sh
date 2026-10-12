#!/bin/bash
set -e

# ==============================================================================
# Danger from the Deep (AmigaOS 4.1) - Mesa 3D Build Script
# Builds both:
#   - DangerFromTheDeep        (high-performance release build, zero debug overhead)
#   - DangerFromTheDeep_Debug  (diagnostic build with verbose logging)
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build_os4"

echo "=== Danger from the Deep - AmigaOS 4.1 Build ==="
echo "Source: ${ROOT_DIR}"
echo "Build:  ${BUILD_DIR}"

BUILD_CMD='
set -e
cd /opt/code/build_os4
make -j$(nproc) dangerdeep

cd src
# 1. Build DangerFromTheDeep_Debug (verbose logging enabled)
sed -e "s/-o dangerdeep_mesa/-o DangerFromTheDeep_Debug/g" link_mesa.sh > link_debug.sh
chmod +x link_debug.sh
./link_debug.sh
mkdir -p /opt/code/dist/debug_symbols
cp DangerFromTheDeep_Debug /opt/code/dist/debug_symbols/DangerFromTheDeep_Debug.sym 2>/dev/null || true
ppc-amigaos-strip -R.comment -R.note DangerFromTheDeep_Debug

# 2. Build DangerFromTheDeep (release, all debug logging disabled)
/opt/ppc-amigaos/bin/ppc-amigaos-g++ -DTIXML_USE_STL -athread=native -Wall -std=gnu++20 -O3 -DDFTD_NO_DEBUG_LOG=1 \
  -Winvalid-pch -include /opt/code/build_os4/src/CMakeFiles/dangerdeep.dir/cmake_pch.hxx \
  -I/opt/code/src -I/opt/code/build_os4 -I/opt/code/build_os4/src -I/opt/code/src/tinyxml \
  -I/opt/code/third_party/build_fftw/include \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/newlib/include/SDL2 \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/common/include/GL4ES \
  -I/opt/code/cmake/gl4es_include \
  -c /opt/code/src/subsim.cpp \
  -o CMakeFiles/dangerdeep.dir/subsim_release.cpp.o

/opt/ppc-amigaos/bin/ppc-amigaos-g++ -DTIXML_USE_STL -athread=native -Wall -std=gnu++20 -O3 -DDFTD_NO_DEBUG_LOG=1 \
  -I/opt/code/src -I/opt/code/build_os4 -I/opt/code/build_os4/src -I/opt/code/src/tinyxml \
  -I/opt/code/third_party/build_fftw/include \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/newlib/include/SDL2 \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/common/include/GL4ES \
  -I/opt/code/cmake/gl4es_include \
  -c /opt/code/src/altivec_detect.cpp \
  -o CMakeFiles/dangerdeep.dir/altivec_detect_release.cpp.o

/opt/ppc-amigaos/bin/ppc-amigaos-g++ -DTIXML_USE_STL -athread=native -Wall -std=gnu++20 -O3 -DDFTD_NO_DEBUG_LOG=1 \
  -Winvalid-pch -include /opt/code/build_os4/src/CMakeFiles/dangerdeep.dir/cmake_pch.hxx \
  -I/opt/code/src -I/opt/code/build_os4 -I/opt/code/build_os4/src -I/opt/code/src/tinyxml \
  -I/opt/code/third_party/build_fftw/include \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/newlib/include/SDL2 \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/common/include/GL4ES \
  -I/opt/code/cmake/gl4es_include \
  -c /opt/code/src/sub_torpedo_display.cpp \
  -o CMakeFiles/dangerdeep.dir/sub_torpedo_display_release.cpp.o

/opt/ppc-amigaos/bin/ppc-amigaos-g++ -DTIXML_USE_STL -athread=native -Wall -std=gnu++20 -O3 -DDFTD_NO_DEBUG_LOG=1 \
  -Winvalid-pch -include /opt/code/build_os4/src/CMakeFiles/dangerdeep.dir/cmake_pch.hxx \
  -I/opt/code/src -I/opt/code/build_os4 -I/opt/code/build_os4/src -I/opt/code/src/tinyxml \
  -I/opt/code/third_party/build_fftw/include \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/newlib/include/SDL2 \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/common/include/GL4ES \
  -I/opt/code/cmake/gl4es_include \
  -c /opt/code/src/global_data.cpp \
  -o CMakeFiles/dangerdeep.dir/global_data_release.cpp.o

/opt/ppc-amigaos/bin/ppc-amigaos-g++ -DTIXML_USE_STL -athread=native -Wall -std=gnu++20 -O3 -DDFTD_NO_DEBUG_LOG=1 \
  -I/opt/code/src -I/opt/code/build_os4 -I/opt/code/build_os4/src -I/opt/code/src/tinyxml \
  -I/opt/code/third_party/build_fftw/include \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/newlib/include/SDL2 \
  -I/opt/ppc-amigaos/ppc-amigaos/SDK/local/common/include/GL4ES \
  -I/opt/code/cmake/gl4es_include \
  -c /opt/code/src/faulthandler.cpp \
  -o CMakeFiles/dangerdeep.dir/faulthandler_release.cpp.o

sed -e "s/CMakeFiles\/dangerdeep\.dir\/subsim\.cpp\.o/CMakeFiles\/dangerdeep\.dir\/subsim_release\.cpp\.o/g" \
    -e "s/CMakeFiles\/dangerdeep\.dir\/altivec_detect\.cpp\.o/CMakeFiles\/dangerdeep\.dir\/altivec_detect_release\.cpp\.o/g" \
    -e "s/CMakeFiles\/dangerdeep\.dir\/sub_torpedo_display\.cpp\.o/CMakeFiles\/dangerdeep\.dir\/sub_torpedo_display_release\.cpp\.o/g" \
    -e "s/CMakeFiles\/dangerdeep\.dir\/global_data\.cpp\.o/CMakeFiles\/dangerdeep\.dir\/global_data_release\.cpp\.o/g" \
    -e "s/CMakeFiles\/dangerdeep\.dir\/faulthandler\.cpp\.o/CMakeFiles\/dangerdeep\.dir\/faulthandler_release\.cpp\.o/g" \
    -e "s/-o dangerdeep_mesa/-o DangerFromTheDeep/g" \
    link_mesa.sh > link_release.sh
chmod +x link_release.sh
./link_release.sh
ppc-amigaos-strip -R.comment -R.note DangerFromTheDeep

# 3. Copy to dist
mkdir -p /opt/code/dist/DangerFromTheDeep
cp DangerFromTheDeep /opt/code/dist/DangerFromTheDeep/
cp DangerFromTheDeep_Debug /opt/code/dist/DangerFromTheDeep/
echo "Successfully built and deployed DangerFromTheDeep and DangerFromTheDeep_Debug"
'

if ! command -v ppc-amigaos-g++ &> /dev/null; then
    echo "ppc-amigaos-g++ not found on host. Running inside Docker..."
    docker run --rm -v "${ROOT_DIR}:/opt/code" walkero/amigagccondocker:os4-gcc11-3.3.0 bash -c "$BUILD_CMD"
else
    echo "Running with local cross-compiler..."
    bash -c "$BUILD_CMD"
fi
