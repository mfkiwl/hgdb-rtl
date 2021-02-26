function(add_include target)
    target_include_directories(${target} PRIVATE
            ../extern/googletest/googletest/include
            ../src)
endfunction()

function(setup_target target)
    target_link_libraries(${target} gtest hgdb-rtl gtest_main)
    add_include(${target})
    gtest_discover_tests(${target})
endfunction()

function(add_test target)
    add_executable(${target} ${target}.cc)
    setup_target(${target})
endfunction()

add_test(test_rtl)

