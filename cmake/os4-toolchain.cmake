set(CMAKE_SYSTEM_NAME AmigaOS)
set(CMAKE_SYSTEM_PROCESSOR ppc)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_C_COMPILER ppc-amigaos-gcc)
set(CMAKE_CXX_COMPILER ppc-amigaos-g++)
set(CMAKE_AR ppc-amigaos-ar)
set(CMAKE_RANLIB ppc-amigaos-ranlib)

set(CMAKE_C_FLAGS_INIT "-athread=native")
set(CMAKE_CXX_FLAGS_INIT "-athread=native")

set(CMAKE_PREFIX_PATH
    /opt/ppc-amigaos/ppc-amigaos/SDK/local/newlib
    /opt/ppc-amigaos/ppc-amigaos/SDK/local/common
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/build_fftw"
    /opt/code/third_party/build_fftw
)

set(CMAKE_FIND_ROOT_PATH 
    /opt/ppc-amigaos/ppc-amigaos/SDK/local/newlib
    /opt/ppc-amigaos/ppc-amigaos/SDK/local/common
    /opt/ppc-amigaos/ppc-amigaos/SDK
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/build_fftw"
    /opt/code/third_party/build_fftw
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
