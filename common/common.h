#pragma once

#include <cstdlib>
#include <cstdarg>
#include <exception>

namespace base {

[[noreturn]] void Die() noexcept;
[[noreturn]] void Die(const char*, ...);
[[noreturn]] void DieV(const char*, va_list vl);
[[noreturn]] void DieNow() noexcept { std::abort(); }

namespace _ {

template <typename R, typename F>
R ExceptionRun(F&& h, const char* expr, const char* file, int line) {
  try {
    return F();
  } catch (std::exception& e) {
    Die("%s.%d]: %s thrown exception %s", file, line, expr, e.what());
  } catch (...) {
    Die("%s.%d]: %s thrown unknown exception", file, line, expr);
  }
}

}  // namespace _
}  // namespace base

