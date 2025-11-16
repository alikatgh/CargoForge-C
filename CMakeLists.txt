cmake_minimum_required(VERSION 3.10)
project(CargoForge-C VERSION 0.2.0 LANGUAGES C)

# C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Platform-specific definitions
if(UNIX)
    add_definitions(-D_POSIX_C_SOURCE=200809L)
endif()

# Source files
set(SOURCES
    main.c
    parser.c
    optimizer.c
    analysis.c
    placement_2d.c
    placement_3d.c
    constraints.c
    visualization.c
)

# Headers
set(HEADERS
    cargoforge.h
    placement_2d.h
    placement_3d.h
    constraints.h
    visualization.h
)

# Main executable
add_executable(cargoforge ${SOURCES} ${HEADERS})
target_link_libraries(cargoforge m)

# Installation
install(TARGETS cargoforge DESTINATION bin)

# Testing
enable_testing()

# Test: parser
add_executable(test_parser tests/test_parser.c parser.c)
target_link_libraries(test_parser m)
add_test(NAME test_parser COMMAND test_parser)

# Test: placement_2d
add_executable(test_placement_2d tests/test_placement_2d.c placement_2d.c)
add_test(NAME test_placement_2d COMMAND test_placement_2d)

# Test: analysis
add_executable(test_analysis tests/test_analysis.c analysis.c)
target_link_libraries(test_analysis m)
add_test(NAME test_analysis COMMAND test_analysis)

# Build options
option(BUILD_WITH_ASAN "Build with AddressSanitizer" OFF)
option(BUILD_WITH_UBSAN "Build with UndefinedBehaviorSanitizer" OFF)

if(BUILD_WITH_ASAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

if(BUILD_WITH_UBSAN)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
endif()

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C Flags: ${CMAKE_C_FLAGS}")
