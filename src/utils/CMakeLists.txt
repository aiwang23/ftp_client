cmake_minimum_required( VERSION 3.20)

project(utils)

include_directories(.)
aux_source_directory(. UTILS_SRC_LIST)
include_directories(${CMAKE_SOURCE_DIR}/3rd/curl/include/)

add_library(utils STATIC ${UTILS_SRC_LIST})
