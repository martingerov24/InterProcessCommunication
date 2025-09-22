cmake_minimum_required(VERSION 3.17)
project(interProcessCommunication CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

option(PROFILE_APPLICATION "Profile Application" ON)

add_subdirectory("${CMAKE_SOURCE_DIR}/protos" protos)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_ZeroMQ QUIET libzmq)
if (NOT PC_ZeroMQ_FOUND)
    pkg_check_modules(PC_ZeroMQ REQUIRED zmq)
endif()

find_path(ZeroMQ_INCLUDE_DIR
    NAMES zmq.hpp
    PATHS ${PC_ZeroMQ_INCLUDE_DIRS} /usr/include /usr/local/include
)
set(ZMQ_HINT_PATHS ${PC_ZeroMQ_LIBRARY_DIRS} /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$" OR CMAKE_LIBRARY_ARCHITECTURE MATCHES "aarch64")
    list(APPEND ZMQ_HINT_PATHS /usr/lib/aarch64-linux-gnu /lib/aarch64-linux-gnu /usr/local/lib/aarch64-linux-gnu)
endif()
find_library(ZeroMQ_LIBRARY
    NAMES zmq
    PATHS ${ZMQ_HINT_PATHS}
)
if (NOT ZeroMQ_INCLUDE_DIR OR NOT ZeroMQ_LIBRARY)
    message(FATAL_ERROR "ZeroMQ not found. Install libzmq (and cppzmq header).")
endif()

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

find_package(Threads REQUIRED)
find_package(CUDAToolkit QUIET)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/sources)
set(INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${SRC_DIR}
    ${SRC_DIR}/third_party
    ${SRC_DIR}/common
    ${ZeroMQ_INCLUDE_DIR}
)

set(SERVER_LIB_SRCS
    ${SRC_DIR}/server/algorithm_runner.cpp
    ${SRC_DIR}/server/application.cpp
    ${SRC_DIR}/ipc_server.cpp
    ${SRC_DIR}/ipc.cpp
)

set(CLIENT_LIB_SRCS
    ${SRC_DIR}/client/application.cpp
    ${SRC_DIR}/ipc_clients.cpp
    ${SRC_DIR}/ipc.cpp
)

set(APP_DEP_NAME app_deps)
add_library(${APP_DEP_NAME} INTERFACE)

target_include_directories(${APP_DEP_NAME} INTERFACE ${INC_DIRS})
target_link_libraries(${APP_DEP_NAME} INTERFACE
    ${ZeroMQ_LIBRARY}
    proto_builder
    fmt::fmt
    spdlog
    Threads::Threads
)

set(COMMON_CORE_NAME common_core)
add_library(${COMMON_CORE_NAME} STATIC
    ${SRC_DIR}/common/error_handling.cpp
)

target_link_libraries(${COMMON_CORE_NAME} PUBLIC ${APP_DEP_NAME})
target_compile_definitions(${COMMON_CORE_NAME} PUBLIC HAVE_CUDA_DRIVER=0)

set(SERVER_CORE_NAME server_common_core)
add_library(${SERVER_CORE_NAME} STATIC
    ${SRC_DIR}/common/error_handling.cpp
)

target_link_libraries(${SERVER_CORE_NAME} PUBLIC ${APP_DEP_NAME})
if (CUDAToolkit_FOUND)
    target_compile_definitions(${SERVER_CORE_NAME} PUBLIC HAVE_CUDA_DRIVER=1)
    target_link_libraries(${SERVER_CORE_NAME} PUBLIC CUDA::cuda_driver)
else()
    target_compile_definitions(${SERVER_CORE_NAME} PUBLIC HAVE_CUDA_DRIVER=0)
endif()

set(SERVER_LIB servercore)
set(CLIENT_STATIC_LIB clientipc_static)
set(CLIENT_SHARED_LIB clientipc)
set(SERVER_TARGET server)
set(CLIENT1_TARGET client_1)
set(CLIENT2_TARGET client_2)

add_library(${SERVER_LIB} STATIC ${SERVER_LIB_SRCS})
target_link_libraries(${SERVER_LIB} PUBLIC ${APP_DEP_NAME} ${SERVER_CORE_NAME})

add_library(${CLIENT_STATIC_LIB} STATIC ${CLIENT_LIB_SRCS})
target_link_libraries(${CLIENT_STATIC_LIB} PUBLIC ${APP_DEP_NAME} ${COMMON_CORE_NAME})

add_library(${CLIENT_SHARED_LIB} SHARED ${CLIENT_LIB_SRCS})
target_link_libraries(${CLIENT_SHARED_LIB} PUBLIC ${APP_DEP_NAME} ${COMMON_CORE_NAME})

add_executable(${SERVER_TARGET} ${SRC_DIR}/server.cpp)
target_link_libraries(${SERVER_TARGET} PRIVATE ${SERVER_LIB} ${APP_DEP_NAME})

add_executable(${CLIENT1_TARGET} ${SRC_DIR}/client_1.cpp)
target_link_libraries(${CLIENT1_TARGET} PRIVATE ${CLIENT_SHARED_LIB} ${APP_DEP_NAME})

add_executable(${CLIENT2_TARGET} ${SRC_DIR}/client_2.cpp)
find_library(DL_LIB dl)
if (DL_LIB)
    target_link_libraries(${CLIENT2_TARGET} PRIVATE ${APP_DEP_NAME} ${COMMON_CORE_NAME} ${DL_LIB})
else()
    target_link_libraries(${CLIENT2_TARGET} PRIVATE ${APP_DEP_NAME} ${COMMON_CORE_NAME})
endif()
set_target_properties(${CLIENT2_TARGET} PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
)

foreach(t ${SERVER_LIB} ${CLIENT_STATIC_LIB} ${CLIENT_SHARED_LIB} ${SERVER_TARGET} ${CLIENT1_TARGET} ${CLIENT2_TARGET} ${COMMON_CORE_NAME} ${SERVER_CORE_NAME})
    if (TARGET ${t})
        target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

message(STATUS "ZeroMQ_INCLUDE_DIR = ${ZeroMQ_INCLUDE_DIR}")
message(STATUS "ZeroMQ_LIBRARY     = ${ZeroMQ_LIBRARY}")

enable_testing()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
    NAME PyTests
    COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests/
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)