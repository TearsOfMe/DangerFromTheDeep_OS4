#!/bin/bash
set -e

# ==============================================================================
# Danger from the Deep (AmigaOS 4.1) - Mesa 3D Build Script
# Builds both:
#   - dangerdeep_mesa       (with verbose debug logging)
#   - dangerdeep_mesa_fast  (high-performance release build)
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build_os4"

echo "=== Danger from the Deep - AmigaOS 4.1 Build ==="
echo "Source: ${ROOT_DIR}"
echo "Build:  ${BUILD_DIR}"

if ! command -v ppc-amigaos-g++ &> /dev/null; then
    echo "ppc-amigaos-g++ not found on host. Running inside Docker..."
    docker run --rm -v "${ROOT_DIR}:/opt/code" walkero/amigagccondocker:os4-gcc11-3.3.0 bash -c "
        set -e
        cd /opt/code/build_os4
        make -j\$(nproc) dangerdeep
        cd src
        ./link_mesa.sh
        ./link_mesa_fast.sh
        ppc-amigaos-strip -R.comment -R.note dangerdeep_mesa
        ppc-amigaos-strip -R.comment -R.note dangerdeep_mesa_fast
        echo 'Successfully linked and stripped dangerdeep_mesa and dangerdeep_mesa_fast'
    "
else
    echo "Running with local cross-compiler..."
    cd "${BUILD_DIR}"
    make -j$(nproc) dangerdeep
    cd src
    ./link_mesa.sh
    ./link_mesa_fast.sh
    ppc-amigaos-strip -R.comment -R.note dangerdeep_mesa
    ppc-amigaos-strip -R.comment -R.note dangerdeep_mesa_fast
    echo "Successfully built dangerdeep_mesa and dangerdeep_mesa_fast"
fi
