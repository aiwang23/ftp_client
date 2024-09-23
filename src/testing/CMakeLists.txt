cmake_minimum_required(VERSION 3.20)

project(testing CXX)

include_directories(../core/)
include_directories(../utils/)
include_directories(${CMAKE_SOURCE_DIR}/3rd/curl/include/)

include_directories(.)

link_directories(${CMAKE_SOURCE_DIR}/3rd/curl/lib)

add_executable(testing01 test01.cpp)

target_link_libraries(
        testing01
        libcurl
        core
        utils
)

add_executable(testing02 test02.cpp)

target_link_libraries(
        testing02
        libcurl
        core
        utils
)

add_executable(testing03 test03.cpp)

target_link_libraries(
        testing03
        libcurl
        core
        utils
)
