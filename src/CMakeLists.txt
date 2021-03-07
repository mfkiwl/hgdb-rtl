function(add_warning_flags TARGET_NAME)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
    else()
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()


add_library(hgdb-rtl rtl.cc vcd.cc)
target_link_libraries(hgdb-rtl PRIVATE slangcompiler vcd)
target_include_directories(hgdb-rtl PUBLIC ../extern/slang/include
        ../extern/slang/external/
        ${CMAKE_CURRENT_BINARY_DIR}/../extern/slang/source
        ../extern/vcd/include)

add_warning_flags(hgdb-rtl)

add_subdirectory(python)