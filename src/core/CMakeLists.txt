cmake_minimum_required(VERSION 3.20)

project(core CXX)

include_directories(../utils)
include_directories(${CMAKE_SOURCE_DIR}/3rd/curl/include/)

include_directories(.)
aux_source_directory(. CORE_SRC_LIST)

add_library(core STATIC ${CORE_SRC_LIST})