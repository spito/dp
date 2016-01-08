cmake_minimum_required(VERSION 2.8.5)
project(DIVINE-NET)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3 -std=c++11")
#set(CMAKE_CXX_COMPILER "${CMAKE_SOURCE_DIR}/clangOrMpi")
set(CMAKE_CXX_COMPILER clang++)

find_package(MPI REQUIRED)

include_directories( SYSTEM bricks )
include_directories( SYSTEM ${MPI_INCLUDE_PATH} )

add_subdirectory(src)

add_executable(test src/test.cpp)
set_target_properties(test PROPERTIES COMPILE_FLAGS "-DBRICK_UNITTEST_REG")
target_link_libraries(test DIVINE-net)

add_executable(brickTest src/test.cpp)
set_target_properties(brickTest PROPERTIES COMPILE_FLAGS "-DBRICK_UNITTEST_REG")

add_executable(demo src/demo.cpp src/meta.cpp)
target_link_libraries(demo DIVINE-net)

add_executable(mpi src/mpi.cpp src/meta.cpp)
set_target_properties( mpi PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}" )
target_link_libraries(mpi ${MPI_LIBRARIES} )
