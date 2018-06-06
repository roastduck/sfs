cmake_minimum_required(VERSION 2.8.12)

project(SFS)

set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3")

if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

file(GLOB_RECURSE SRCS ${CMAKE_BINARY_DIR}/src/*.cpp)
add_executable(sfs ${SRCS})
target_link_libraries(sfs -lfuse -lgit2 -lpthread)
set_target_properties(
    sfs
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED true
)
