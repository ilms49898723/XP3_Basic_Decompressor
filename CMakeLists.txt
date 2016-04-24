cmake_minimum_required(VERSION 3.5)

project(XP3_Decompressor)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Os -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -Wall")

set(SOURCE_FILES main.cpp)

add_executable(XP3_Decompressor ${SOURCE_FILES})
