cmake_minimum_required(VERSION 4.0)

project(libkas)


if ( PROJECT_IS_TOP_LEVEL )
    set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
    set(CMAKE_C_STANDARD_REQUIRED YES)
    set(CMAKE_C_STANDARD 99)
    file(
        CREATE_LINK
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
        SYMBOLIC COPY_ON_ERROR
    )
endif()

add_library(kas_alloc INTERFACE)
add_library(kas_search STATIC kas_search.c)
add_library(kas_parse STATIC kas_parse.c)
target_link_libraries(kas_parse PRIVATE kas_alloc)
add_library(kas_kreader_wrap STATIC kas_kreader_wrap.c)
target_link_libraries(kas_parse PRIVATE kas_alloc)
