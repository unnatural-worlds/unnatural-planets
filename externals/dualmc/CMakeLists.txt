cmake_minimum_required(VERSION 3.10)

message(STATUS "----------------------------------------------------------------")
message(STATUS "dualmc")
message(STATUS "----------------------------------------------------------------")

project(dualmc C)

add_library(dualmc STATIC "dualmc/include/dualmc.h" "dummy.c")

target_include_directories(dualmc PUBLIC dualmc/include)

cage_ide_category(dualmc unnatural-planets/externals)
cage_ide_sort_files(dualmc)

configure_file("dualmc/LICENSE" "${CMAKE_BINARY_DIR}/result/redist/licenses/dualmc" COPYONLY)
