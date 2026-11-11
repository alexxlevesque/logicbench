include(FetchContent)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(logicbench_tests
    SignalTest.cpp
    GateTest.cpp
    CircuitTest.cpp
    SimulatorTest.cpp
    SubCircuitTest.cpp
    SerializerTest.cpp
    TruthTableTest.cpp
    PerformanceTest.cpp
    ExamplesTest.cpp
    FlipFlopTest.cpp
    MultiplexerTest.cpp
    DecoderTest.cpp
    DisplayTest.cpp
    FragmentTest.cpp
)

# The examples live in the repository, so the test reads them from there.
target_compile_definitions(logicbench_tests PRIVATE
    LOGICBENCH_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")

target_link_libraries(logicbench_tests PRIVATE logicbench_engine GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(logicbench_tests)

# The GUI smoke test drives real mouse events through the view, so it needs
# Qt and the application's own sources. Off by default: the engine suite is
# what runs on every commit, and this one needs a Qt build.
if(LOGICBENCH_BUILD_GUI_TESTS AND LOGICBENCH_BUILD_APP)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

    # qt_standard_project_setup() ran in app/, and its effect is scoped to that
    # directory, so moc has to be switched on again here.
    set(CMAKE_AUTOMOC ON)

    qt_add_executable(logicbench_gui_tests
        GuiSmokeTest.cpp
        ${LOGICBENCH_APP_SOURCES}
    )
    target_include_directories(logicbench_gui_tests PRIVATE ${LOGICBENCH_APP_INCLUDE_DIR})
    target_compile_definitions(logicbench_gui_tests PRIVATE
        LOGICBENCH_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples")

    # A bare executable on macOS launches as a background process and cannot
    # take focus, which matters when the suite is run on a real display rather
    # than offscreen. Bundling it makes it an ordinary app.
    set_target_properties(logicbench_gui_tests PROPERTIES MACOSX_BUNDLE TRUE)
    target_link_libraries(logicbench_gui_tests
        PRIVATE logicbench_engine Qt6::Widgets Qt6::Test)

    add_test(NAME gui_smoke
             COMMAND logicbench_gui_tests -platform offscreen)
endif()
