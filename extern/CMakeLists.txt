include(ExternalProject)
ExternalProject_add(slang
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/slang"
        BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/slang
        INSTALL_COMMAND ""
        TEST_COMMAND ""
)
SET(libslang ${CMAKE_CURRENT_BINARY_DIR}/libslangcompiler.a)