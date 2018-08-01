cmake_minimum_required(VERSION 3.2)
project(unnatural-planets CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cage/cmake")
include(cage_all)
cage_default_init()

add_subdirectory(externals/cage)

file(GLOB_RECURSE unplanets-sources "sources/*")
add_executable(unplanets ${unplanets-sources})
target_link_libraries(unplanets cage-client)
cage_ide_groups(unplanets unplanets)
cage_ide_working_dir_in_place(unplanets)
