add_library(hgdb-rtl SHARED rtl.cc)
target_link_libraries(hgdb-rtl PRIVATE slangcompiler)
target_include_directories(hgdb-rtl PUBLIC ../extern/slang/include
        ../extern/slang/external/
        ${CMAKE_CURRENT_BINARY_DIR}/../extern/slang/source)