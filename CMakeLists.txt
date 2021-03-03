cmake_minimum_required(VERSION 3.12)
project(hgdb_rtl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# slang specific flags
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_definitions(DEBUG)
endif()

add_subdirectory(extern)
add_subdirectory(src)

add_subdirectory(extern/googletest)
include(GoogleTest)
include (CTest)
enable_testing()
add_subdirectory(tests)
