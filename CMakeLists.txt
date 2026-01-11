cmake_minimum_required(VERSION 4.0)
project(NBodyProblem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)

find_package(MPI REQUIRED)

add_executable(NBodyProblem
    main.cpp
    nbody.h
    nbody_sequential.cpp
    nbody_threads.cpp
    nbody_mpi.cpp
    nbody_opencl.cpp
)

# Enable MPI in the code
target_compile_definitions(NBodyProblem PRIVATE USE_MPI)

target_include_directories(NBodyProblem PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Try to find OpenCL SDK, otherwise use local headers + system OpenCL.dll
find_package(OpenCL QUIET)

if(OpenCL_FOUND)
    target_link_libraries(NBodyProblem 
        MPI::MPI_CXX
        OpenCL::OpenCL
    )
else()
    # Use local OpenCL headers and link to system OpenCL
    # On Windows, OpenCL.dll is usually present if you have GPU drivers
    if(WIN32)
        # Find OpenCL.lib from GPU vendor SDK or Windows SDK
        find_library(OPENCL_LIBRARY 
            NAMES OpenCL
            PATHS 
                "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1/lib/x64"
                "$ENV{CUDA_PATH}/lib/x64"
                "$ENV{AMDAPPSDKROOT}/lib/x86_64"
                "$ENV{INTELOCLSDKROOT}/lib/x64"
        )
        if(OPENCL_LIBRARY)
            target_link_libraries(NBodyProblem MPI::MPI_CXX ${OPENCL_LIBRARY})
        else()
            message(WARNING "OpenCL library not found. OpenCL benchmark will fail at runtime.")
            target_link_libraries(NBodyProblem MPI::MPI_CXX)
        endif()
    else()
        target_link_libraries(NBodyProblem MPI::MPI_CXX)
        message(WARNING "OpenCL not found. OpenCL features will not work.")
    endif()
endif()
