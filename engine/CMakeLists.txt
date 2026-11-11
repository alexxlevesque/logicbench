# The simulation engine: the whole product minus the interface.
#
# Nothing in here links or includes Qt, which is what lets the tests run
# headless and lets two people work on logic and on the GUI without waiting for
# each other. cmake/AssertEngineIsQtFree.cmake enforces it.

include(FetchContent)

# nlohmann/json, single header. Fetched as a bare file rather than as a CMake
# project so the build does not inherit another project's policy settings.
set(LOGICBENCH_JSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/json-include")
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp")
    set(LOGICBENCH_JSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party")
    message(STATUS "Using vendored nlohmann/json")
else()
    FetchContent_Declare(nlohmann_json_header
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp
        URL_HASH SHA256=aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63
        DOWNLOAD_NO_EXTRACT TRUE)
    FetchContent_MakeAvailable(nlohmann_json_header)
    file(MAKE_DIRECTORY "${LOGICBENCH_JSON_INCLUDE_DIR}/nlohmann")
    file(COPY "${nlohmann_json_header_SOURCE_DIR}/json.hpp"
         DESTINATION "${LOGICBENCH_JSON_INCLUDE_DIR}/nlohmann")
endif()

add_library(lb_json INTERFACE)
target_include_directories(lb_json SYSTEM INTERFACE "${LOGICBENCH_JSON_INCLUDE_DIR}")

add_library(logicbench_engine STATIC
    src/Signal.cpp
    src/core/Component.cpp
    src/core/Gates.cpp
    src/core/IoComponents.cpp
    src/core/FlipFlop.cpp
    src/core/Multiplexer.cpp
    src/core/Circuit.cpp
    src/core/Simulator.cpp
    src/core/SubCircuit.cpp
    src/services/ComponentFactory.cpp
    src/services/TruthTable.cpp
    src/services/CircuitSerializer.cpp
)

target_include_directories(logicbench_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(logicbench_engine PRIVATE lb_json)
target_compile_features(logicbench_engine PUBLIC cxx_std_17)
