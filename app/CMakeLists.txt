# The Qt 6 interface. This is the only target that knows Qt exists.

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

# Listed once so the optional GUI test can compile the same sources.
set(LOGICBENCH_APP_SOURCES
    src/MainWindow.cpp
    src/CircuitDocument.cpp
    src/Commands.cpp
    src/CircuitScene.cpp
    src/ComponentItem.cpp
    src/WireItem.cpp
    src/PaletteDock.cpp
    src/PropertiesDock.cpp
    src/WaveformDock.cpp
    src/TruthTableDialog.cpp
    src/SubcircuitDialog.cpp
    src/CircuitClipboard.cpp
    src/AppSettings.cpp
    src/AutosaveManager.cpp
    src/ExampleCatalog.cpp
    src/QuickStartTracker.cpp
    src/ExpressionDialog.cpp
    src/SimplifyDialog.cpp
    src/SpecCheckDialog.cpp
    src/BlockLibraryDialog.cpp
    src/ExportImageDialog.cpp
    src/ExamplesDialog.cpp
)
list(TRANSFORM LOGICBENCH_APP_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
set(LOGICBENCH_APP_SOURCES ${LOGICBENCH_APP_SOURCES} PARENT_SCOPE)
set(LOGICBENCH_APP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src PARENT_SCOPE)

qt_add_executable(logicbench
    src/main.cpp
    ${LOGICBENCH_APP_SOURCES}
)

target_include_directories(logicbench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# The Examples window reads the bundled circuits from the source tree until
# FR-3 packages them as resources.
target_compile_definitions(logicbench PRIVATE
    LOGICBENCH_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")
target_link_libraries(logicbench PRIVATE logicbench_engine Qt6::Widgets)

set_target_properties(logicbench PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)
