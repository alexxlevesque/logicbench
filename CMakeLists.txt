cmake_minimum_required(VERSION 3.21)
project(LogicBench
        VERSION 0.4.0
        DESCRIPTION "Digital-logic simulator with a GUI-free simulation engine"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(LOGICBENCH_BUILD_TESTS "Build the engine test suite" ON)
option(LOGICBENCH_BUILD_APP   "Build the Qt 6 application"  ON)
option(LOGICBENCH_BUILD_EXAMPLES "Build the example-circuit generator" OFF)
option(LOGICBENCH_BUILD_GUI_TESTS "Build the headless GUI smoke test (needs Qt)" OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Three targets, as the project plan requires:
#   logicbench_engine  static library, no Qt anywhere in it
#   logicbench         Qt 6 Widgets application
#   logicbench_tests   GoogleTest suite over the engine
add_subdirectory(engine)

if(LOGICBENCH_BUILD_APP)
    add_subdirectory(app)
endif()

if(LOGICBENCH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LOGICBENCH_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# FR: "The engine library has no Qt includes, enforced by a CMake target that
# does not link Qt."  The check below fails the build if anyone ever adds one.
include(cmake/AssertEngineIsQtFree.cmake)
