# Developer utility, off by default: regenerates the .lbc files in this folder.
# Not one of the project's three shipped targets.
add_executable(logicbench_examples generate_examples.cpp)
target_link_libraries(logicbench_examples PRIVATE logicbench_engine)
