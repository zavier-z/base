list (APPEND EVENT_SRCS
  basic.cc
)

add_library(base_event STATIC ${EVENT_SRCS})
target_link_libraries(base_event
  fmt
  base_comm
)

target_include_directories(base_event
  PUBLIC
    ${CMAKE_SOURCE_DIR}/third-party/fmt/include
)

if(BASE_BUILD_TESTS)
  base_test("promise-test.cc")
endif(BASE_BUILD_TESTS)
