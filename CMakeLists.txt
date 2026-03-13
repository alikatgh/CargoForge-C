cmake_minimum_required(VERSION 3.10)
project(CargoForge-C VERSION 3.0.0 LANGUAGES C)

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

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- Library sources (core engine) ---
set(LIB_SOURCES
    src/parser.c
    src/analysis.c
    src/placement_3d.c
    src/constraints.c
    src/json_output.c
    src/hydrostatics.c
    src/tanks.c
    src/longitudinal_strength.c
    src/imdg.c
    src/libcargoforge.c
)

# --- CLI + server sources ---
set(CLI_SOURCES
    src/main.c
    src/cli.c
    src/visualization.c
    src/server.c
)

set(HEADERS
    include/cargoforge.h
    include/cli.h
    include/placement_3d.h
    include/constraints.h
    include/visualization.h
    include/json_output.h
    include/hydrostatics.h
    include/tanks.h
    include/longitudinal_strength.h
    include/imdg.h
    include/libcargoforge.h
    include/server.h
)

# --- Static library ---
add_library(cargoforge_static STATIC ${LIB_SOURCES})
set_target_properties(cargoforge_static PROPERTIES
    OUTPUT_NAME cargoforge
    POSITION_INDEPENDENT_CODE ON
)
target_link_libraries(cargoforge_static m)

# --- Shared library ---
add_library(cargoforge_shared SHARED ${LIB_SOURCES})
set_target_properties(cargoforge_shared PROPERTIES
    OUTPUT_NAME cargoforge
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_link_libraries(cargoforge_shared m)

# --- Main executable ---
add_executable(cargoforge ${CLI_SOURCES} ${LIB_SOURCES} ${HEADERS})
target_link_libraries(cargoforge m)

# --- Installation ---
install(TARGETS cargoforge DESTINATION bin)
install(TARGETS cargoforge_static cargoforge_shared DESTINATION lib)
install(FILES include/libcargoforge.h DESTINATION include)

# --- Testing ---
enable_testing()

add_executable(test_parser tests/test_parser.c src/parser.c src/hydrostatics.c src/tanks.c src/longitudinal_strength.c)
target_link_libraries(test_parser m)
add_test(NAME test_parser COMMAND test_parser)

add_executable(test_analysis tests/test_analysis.c src/analysis.c src/hydrostatics.c src/tanks.c src/longitudinal_strength.c)
target_link_libraries(test_analysis m)
add_test(NAME test_analysis COMMAND test_analysis)

add_executable(test_constraints tests/test_constraints.c src/constraints.c src/placement_3d.c src/imdg.c)
target_link_libraries(test_constraints m)
add_test(NAME test_constraints COMMAND test_constraints)

add_executable(test_hydrostatics tests/test_hydrostatics.c src/hydrostatics.c)
target_link_libraries(test_hydrostatics m)
add_test(NAME test_hydrostatics COMMAND test_hydrostatics)

add_executable(test_tanks tests/test_tanks.c src/tanks.c)
target_link_libraries(test_tanks m)
add_test(NAME test_tanks COMMAND test_tanks)

add_executable(test_longitudinal_strength tests/test_longitudinal_strength.c src/longitudinal_strength.c)
target_link_libraries(test_longitudinal_strength m)
add_test(NAME test_longitudinal_strength COMMAND test_longitudinal_strength)

add_executable(test_imdg tests/test_imdg.c src/imdg.c)
target_link_libraries(test_imdg m)
add_test(NAME test_imdg COMMAND test_imdg)

add_executable(test_library tests/test_library.c)
target_link_libraries(test_library cargoforge_static)
add_test(NAME test_library COMMAND test_library)

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

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C Flags: ${CMAKE_C_FLAGS}")
