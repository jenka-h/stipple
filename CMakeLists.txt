cmake_minimum_required(VERSION 3.20)

option(STIPPLE_ENABLE_CUDA "Build CUDA backend" OFF)
option(STIPPLE_ENABLE_GUI "Build simple Qt6 GUI" OFF)

if(STIPPLE_ENABLE_CUDA)
    project(stipple LANGUAGES CXX CUDA)
else()
    project(stipple LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc videoio)
find_package(OpenMP)

set(STIPPLE_CORE_SOURCES
    src/image.cpp
    src/renderer.cpp
    src/animation.cpp
    src/point.cpp
    src/config.cpp
    src/lloyd.cpp
    src/logic/serial.cpp
    src/logic/openmp.cpp
    src/logic/simd.cpp
)

if(STIPPLE_ENABLE_CUDA)
    list(APPEND STIPPLE_CORE_SOURCES src/logic/cuda.cu)
else()
    list(APPEND STIPPLE_CORE_SOURCES src/logic/cuda.cpp)
endif()

add_library(stipple_core STATIC ${STIPPLE_CORE_SOURCES})

add_executable(stipple src/main.cpp)

target_include_directories(stipple_core PUBLIC
    include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(stipple_core PUBLIC ${OpenCV_LIBS})

target_link_libraries(stipple PRIVATE stipple_core)

if(OpenMP_CXX_FOUND)
    target_link_libraries(stipple_core PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(stipple_core PUBLIC STIPPLE_WITH_OPENMP=1)
endif()

if(STIPPLE_ENABLE_CUDA)
    target_compile_definitions(stipple_core PUBLIC STIPPLE_WITH_CUDA=1)
    set_target_properties(stipple_core PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(stipple_core PRIVATE -O3 -march=native)
endif()

if(STIPPLE_ENABLE_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    add_executable(stipple_gui src/gui.cpp)

    target_link_libraries(stipple_gui PRIVATE
        stipple_core
        Qt6::Widgets
    )
endif()
