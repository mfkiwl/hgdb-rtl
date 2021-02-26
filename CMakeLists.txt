cmake_minimum_required(VERSION 3.12)
project(hgdb_rtl)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(tools)

add_subdirectory(extern/googletest)
include(GoogleTest)
include (CTest)
enable_testing()
add_subdirectory(tests)
