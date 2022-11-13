#pragma once

#include <cstdlib>
#include <cstdarg>
#include <string>
#include <exception>

namespace base {

std::string GetStackTrace();

[[noreturn]] void Die() noexcept;
[[noreturn]] void DieNow() noexcept;
[[noreturn]] void Die(const char*, ...);
[[noreturn]] void DieV(const char*, va_list vl);

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

