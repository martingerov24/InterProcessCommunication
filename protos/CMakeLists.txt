cmake_minimum_required(VERSION 3.21)
project(proto_builder LANGUAGES CXX)

option(PROTO_BUILD_GRPC "Generate gRPC stubs" OFF)

find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

set(_PROTOBUF_TARGET "")
if (TARGET protobuf::libprotobuf)
    set(_PROTOBUF_TARGET protobuf::libprotobuf)
elseif (TARGET Protobuf::libprotobuf)
    set(_PROTOBUF_TARGET Protobuf::libprotobuf)
elseif (TARGET protobuf::protobuf)
    set(_PROTOBUF_TARGET protobuf::protobuf)
else()
    message(FATAL_ERROR "Could not find an imported protobuf target after find_package(Protobuf).")
endif()

if (PROTO_BUILD_GRPC)
    find_package(gRPC CONFIG REQUIRED)
endif()

file(GLOB_RECURSE PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")
if (NOT PROTO_FILES)
    message(FATAL_ERROR "No .proto files found under ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

set(GRPC_SRCS)
set(GRPC_HDRS)
if (PROTO_BUILD_GRPC)
    grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_FILES})
endif()

add_library(${PROJECT_NAME}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${_PROTOBUF_TARGET})

if (PROTO_BUILD_GRPC)
    target_link_libraries(${PROJECT_NAME} PUBLIC gRPC::grpc++)
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_14)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)