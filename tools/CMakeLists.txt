add_executable(hgdb-rtl-bin hgdb-rtl.cc)
target_link_libraries(hgdb-rtl-bin PRIVATE hgdb-rtl -static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive)
target_include_directories(hgdb-rtl-bin PUBLIC ../src)
set_target_properties(hgdb-rtl-bin PROPERTIES OUTPUT_NAME hgdb-rtl)