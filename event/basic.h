#pragma once

#include <common/error.h>
#include <fmt/format.h>

#define EVENT_ERROR_LIST(__) \
  __(kErrorEventPromiseAny, "promise any operation failed")

namespace base {
namespace event {

enum EventError {
#define __(A, B) A,
  EVENT_ERROR_LIST(__)
#undef __
};

const Error::Category *Cat();

inline Error Err(EventError e) { return Error{Cat(), e}; }

template <typename... T>
Error Err(EventError e, const T &... args) {
  auto msg = fmt::format(args...);
  return Error{Cat(), e, std::move(msg)};
}

}  // namespace event
}  // namespace base
