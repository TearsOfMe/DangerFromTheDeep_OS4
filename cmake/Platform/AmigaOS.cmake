# AmigaOS platform module for CMake

set(AMIGAOS 1)

set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
set(CMAKE_EXECUTABLE_SUFFIX "")

set(CMAKE_DL_LIBS "")

include(Platform/UnixPaths)
