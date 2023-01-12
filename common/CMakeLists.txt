list (APPEND COMMON_SRCS
  common.cc
  timer-wheel.cc
)

add_library(base_comm STATIC ${COMMON_SRCS})
target_link_libraries(base_comm
  fmt
  absl_stacktrace
  absl_symbolize
)

target_include_directories(base_comm
  PUBLIC
    ${CMAKE_SOURCE_DIR}/third-party/fmt/include
)

