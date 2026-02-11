cmake_minimum_required(VERSION 3.1)
project(fstra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找OpenMP
find_package(OpenMP REQUIRED)

# some specific compiler definitions
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fcolor-diagnostics" HAS_FCOLOR_DIAGNOSTICS)
if (HAS_FCOLOR_DIAGNOSTICS)
  add_definitions(-fcolor-diagnostics)
endif()

# 添加OpenMP编译标志
if(OpenMP_CXX_FOUND)
    add_definitions(${OpenMP_CXX_FLAGS})
endif()

include_directories(include)

add_subdirectory(lib)
add_subdirectory(tests)
add_subdirectory(work)