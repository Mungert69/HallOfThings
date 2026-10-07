cmake_minimum_required(VERSION 3.16)
project(halls_of_the_things_original_maze_v2 VERSION 0.5.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(HALLS_SMALL_BINARY "Optimize the release executable for size" ON)


add_library(halls_core
    src/game.c
)

target_include_directories(halls_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(halls_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(halls_original_maze_v2
    src/main.c
)
set_target_properties(halls_original_maze_v2 PROPERTIES OUTPUT_NAME "halls-original-maze-v2")

target_link_libraries(halls_original_maze_v2 PRIVATE
    halls_core
    PkgConfig::SDL2
)

target_compile_options(halls_original_maze_v2 PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(HALLS_SMALL_BINARY)
    target_compile_options(halls_core PRIVATE
        $<$<CONFIG:Release>:-Os>
        $<$<CONFIG:Release>:-ffunction-sections>
        $<$<CONFIG:Release>:-fdata-sections>
    )
    target_compile_options(halls_original_maze_v2 PRIVATE
        $<$<CONFIG:Release>:-Os>
        $<$<CONFIG:Release>:-ffunction-sections>
        $<$<CONFIG:Release>:-fdata-sections>
    )
    target_link_options(halls_original_maze_v2 PRIVATE
        $<$<CONFIG:Release>:-Wl,--gc-sections>
        $<$<CONFIG:Release>:-s>
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(halls_tests
        tests/test_game.c
    )
    target_link_libraries(halls_tests PRIVATE halls_core)
    add_test(NAME core COMMAND halls_tests)
endif()
