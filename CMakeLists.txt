cmake_minimum_required(VERSION 3.5)

project(XP3_Decompressor)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Os -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -Wall")

set(SOURCE_FILES main.cpp zpipe.cpp zpipe.h)

add_library(zlib STATIC IMPORTED)

set_target_properties(zlib PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/zlib/libzlibstatic.a)

add_executable(XP3_Decompressor ${SOURCE_FILES})

target_link_libraries(XP3_Decompressor zlib)
