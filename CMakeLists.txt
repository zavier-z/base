cmake_minimum_required(VERSION 3.12)

project(base VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -rdynamic")

option(BASE_BUILD_TESTS "build base unit tests" ON)

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
)

# add_library(base STATIC "")
# target_sources(base
#   PUBLIC
#     "common/bind.h"
#     "common/check.h"
#     "common/common.h"
#     "common/concept.h"
#     "common/error.h"
#     "common/result.h"
#     "common/timer-wheel.h"
#     "common/trait.h"
#     "common/macros.h"
#     "common/no-destructor.h"
#     "event/promise.h"
#   PRIVATE
#     "common/common.cc"
#     "common/timer-wheel.cc"
#     "event/basic.h"
#     "event/basic.cc"
# )
# 
# target_include_directories(base
#   PUBLIC
#   ${CMAKE_CURRENT_LIST_DIR}
#   ${CMAKE_SOURCE_DIR}/third-party/abseil-cpp
#   ${CMAKE_SOURCE_DIR}/third-party/fmt/include
#   ${CMAKE_SOURCE_DIR}/third-party/catch2
# )
# 
# add_subdirectory(third-party)
# 
# target_link_libraries(base
#  PUBLIC 
#     absl_stacktrace
#     absl_symbolize
#     fmt
# )

# add_executable(main main.cc)
# target_link_libraries(main base)

if(BASE_BUILD_TESTS)
  enable_testing()

  function(base_test test_file)
    get_filename_component(test_target_name "${test_file}" NAME_WE)

    add_executable("${test_target_name}" "")
    target_sources("${test_target_name}"
      PRIVATE
      "${test_file}"
    )

    target_include_directories("${test_target_name}"
      PRIVATE
        ${CMAKE_SOURCE_DIR}/third-party/catch2
    )
    
    target_link_libraries("${test_target_name}" base_comm base_event)

    add_test(NAME "${test_target_name}" COMMAND "${test_target_name}")
  endfunction(base_test)
endif(BASE_BUILD_TESTS)

add_subdirectory(third-party)
add_subdirectory(common)
add_subdirectory(event)

add_executable(main main.cc)
target_link_libraries(main base_comm base_event)

