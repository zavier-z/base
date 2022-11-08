cmake_minimum_required(VERSION 3.12)

project(base)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Werror")

add_executable(base main.cc)

target_include_directories(${PROJECT_NAME}
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(third-party)
