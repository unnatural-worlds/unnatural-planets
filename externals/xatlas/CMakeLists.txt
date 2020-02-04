cmake_minimum_required(VERSION 3.10)

message(STATUS "----------------------------------------------------------------")
message(STATUS "xatlas")
message(STATUS "----------------------------------------------------------------")

project(xatlas CXX)

add_library(xatlas STATIC "xatlas/xatlas.h" "xatlas/xatlas.cpp")

target_include_directories(xatlas PUBLIC xatlas)

cage_ide_category(xatlas unnatural-planets/externals)
cage_ide_sort_files(xatlas)

configure_file("xatlas/LICENSE" "${CMAKE_BINARY_DIR}/result/redist/licenses/xatlas" COPYONLY)
