add_library(hgdb-rtl rtl.cc vcd.cc)
target_link_libraries(hgdb-rtl PRIVATE slangcompiler vcd)
target_include_directories(hgdb-rtl PUBLIC ../extern/slang/include
        ../extern/slang/external/
        ${CMAKE_CURRENT_BINARY_DIR}/../extern/slang/source
        ../extern/vcd/include)

add_subdirectory(python)