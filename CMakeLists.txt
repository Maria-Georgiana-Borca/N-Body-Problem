cmake_minimum_required(VERSION 4.0)
project(Project)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "/opt/homebrew")

find_package(MPI REQUIRED)

add_executable(Project main.cpp
        nbody.h
        nbody_sequential.cpp
        nbody_threads.cpp
        nbody_mpi.cpp)

target_link_libraries(Project PRIVATE MPI::MPI_CXX)
target_include_directories(Project PRIVATE ${MPI_CXX_INCLUDE_DIRS})