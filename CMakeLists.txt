cmake_minimum_required(VERSION 3.2)
project(unnatural-planets CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cage/cmake")
include(cage_all)
cage_default_init()

add_subdirectory(externals/cage)

file(GLOB_RECURSE unnatural-planets-sources "sources/*")
add_executable(unnatural-planets ${unnatural-planets-sources})
target_include_directories(unnatural-planets PRIVATE "externals/dualmc/include" "externals/xatlas" "externals/ng_mesh")
target_sources(unnatural-planets PRIVATE
	"externals/dualmc/include/dualmc.h"
	"externals/xatlas/xatlas.h" "externals/xatlas/xatlas.cpp"
	"externals/ng_mesh/ng_mesh.cpp" "externals/ng_mesh/ng_mesh.h" "externals/ng_mesh/qef_simd.h"
)
if(NOT MSVC)
	target_link_libraries(unnatural-planets pthread)
endif()
target_link_libraries(unnatural-planets cage-core cage-client)
cage_ide_category(unnatural-planets unnatural-planets)
cage_ide_sort_files(unnatural-planets)
cage_ide_working_dir_in_place(unnatural-planets)
