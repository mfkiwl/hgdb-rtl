add_library(hgdb-rtl rtl.cc)
target_link_libraries(hgdb-rtl libslang)
target_include_directories(hgdb-rtl PRIVATE ../extern/slang/include)