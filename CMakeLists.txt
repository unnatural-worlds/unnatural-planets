cmake_minimum_required(VERSION 3.10)
project(unnatural-planets CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cage/cmake")
include(cage_all)
cage_default_init()

add_subdirectory(externals)

file(GLOB_RECURSE unnatural-planets-sources "sources/*")
add_executable(unnatural-planets ${unnatural-planets-sources})
target_link_libraries(unnatural-planets cage-core cage-engine dualmc pmp xatlas)
cage_ide_category(unnatural-planets unnatural-planets)
cage_ide_sort_files(unnatural-planets)
cage_ide_working_dir_in_place(unnatural-planets)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/result/output")
