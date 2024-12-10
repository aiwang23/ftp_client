cmake_minimum_required(VERSION 3.20)
project(FileMover CXX)

set(CMAKE_CXX_STANDARD 17)

# 可执行文件在根目录的bin
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/)
# 编译的库在根目录的lib
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib/)

add_subdirectory(src)
