# file(GLOB FILENAMES *.cpp)

# foreach(filename ${FILENAMES})
#   get_filename_component(basename ${filename} NAME_WE)
#   add_executable(${basename} ${filename})
#   target_link_libraries(${basename} PUBLIC mockturtle)
#   target_link_libraries( ${basename} PUBLIC Eigen3::Eigen)
#   target_link_libraries(${basename} PUBLIC verilogSim)

#   if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${basename}.cmake")
#     include(${basename}.cmake)
#   endif()
# endforeach()

find_package(OpenMP REQUIRED)

add_executable(fstra
    main.cpp
    iverilog_simulator.cpp
    fstra.cpp
    parse_verilog.cpp
)

target_link_libraries(fstra PUBLIC mockturtle)
target_link_libraries( fstra PUBLIC Eigen3::Eigen)
target_link_libraries(fstra PUBLIC OpenMP::OpenMP_CXX)
# target_link_libraries(fstra PUBLIC verilogSim)

