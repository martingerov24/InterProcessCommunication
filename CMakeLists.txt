cmake_minimum_required(VERSION 3.17)
project(interProcessCommunication)

set(CMAKE_BUILD_TYPE Debug)

option(PROFILE_APPLICATION "Profile Application" ON)

add_subdirectory("${CMAKE_SOURCE_DIR}/protos" protos)

find_package(PkgConfig)

pkg_check_modules(PC_ZeroMQ QUIET zmq)
## use the hint from above to find where 'zmq.hpp' is located
find_path(ZeroMQ_INCLUDE_DIR
    NAMES zmq.hpp
    PATHS ${PC_ZeroMQ_INCLUDE_DIRS}
)

## use the hint from above to find the location of libzmq
find_library(ZeroMQ_LIBRARY
    NAMES zmq
    PATHS ${PC_ZeroMQ_LIBRARY_DIRS}
)

include(FetchContent)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG "9.1.0"
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG "v1.11.0"
)
FetchContent_MakeAvailable(spdlog)

find_package(CUDAToolkit REQUIRED)
find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/docs/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs
        COMMENT "Generating Doxygen XML/HTML"
    )
endif()

set(SOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sources)
set(SOUCES_INCLUDE_DIR
    ${SOURCES_DIR}/third_party
    ${SOURCES_DIR}/common
    ${SOURCES_DIR}
)

set(SOURCES
    ${SOURCES_DIR}/server/algorithm_runner.cpp
    ${SOURCES_DIR}/server/application.cpp
    ${SOURCES_DIR}/client/application.cpp
    ${SOURCES_DIR}/common/error_handling.cpp
    ${SOURCES_DIR}/ipc.cpp
    ${SOURCES_DIR}/main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SOUCES_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ZeroMQ_INCLUDE_DIR}
    ${CUDAToolkit_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${PC_ZeroMQ_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${ZeroMQ_LIBRARY}
    proto_builder
    stdc++fs
    fmt::fmt
    spdlog
    ${CUDAToolkit_LIBS}
    cuda
)
