#pragma once

#include <cstdarg>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string>
#include <fmt/format.h>
#include <fmt/printf.h>

#include "check.h"
#include "concept.h"
#include "macros.h"

#define NO_EXCEPT(...)                                                        \
  ::base::_::ExceptionRun<decltype(__VA_ARGS__)>(                             \
      [&]() -> decltype(__VA_ARGS__) { return (__VA_ARGS__); }, #__VA_ARGS__, \
      __FILE__, __LINE__)

#if defined(__COUNTER__)
#define __DEFERABLE_RUNNER_INST_NAME(arg) CONCATE(__defer, arg)
#define DEFERABLE_RUNNER_INST_NAME __DEFERABLE_RUNNER_INST_NAME(__COUNTER__)
#else
#define __DEFERABLE_RUNNER_INST_NAME(arg) CONCATE(__defer, arg)
#define DEFERABLE_RUNNER_INST_NAME __DEFERABLE_RUNNER_INST_NAME(__LINE__)
#endif

#define DEFER(...) \
  auto DEFERABLE_RUNNER_INST_NAME = ::base::_::MkDeferableRunner(__VA_ARGS__)

namespace base {

std::string GetStackTrace();

[[noreturn]] inline void DieNow() noexcept { std::abort(); }
[[noreturn]] inline void Die() noexcept {
  fmt::fprintf(stderr, "bt: \n{}\n", GetStackTrace());
  DieNow();
}

template <typename... ARGS>
[[noreturn]] inline void Die(const char* fmt, const ARGS&... args) {
  auto msg = fmt::format(fmt, args...);
  fmt::fprintf(stderr, "[die]: {}", msg);
  Die();
}

namespace _ {

template <typename R, typename F>
R ExceptionRun(F&& h, const char* expr, const char* file, int line) {
  try {
    return h();
  } catch (std::exception& e) {
    Die("%s.%d]: %s thrown exception %s", file, line, expr, e.what());
  } catch (...) {
    Die("%s.%d]: %s thrown unknown exception", file, line, expr);
  }
}

DEFINE_CONCEPT(HasStdSwap, void (_::*)(_&), &_::swap);
DEFINE_CONCEPT(HasUserSwap, void (_::*)(_*), &_::Swap);

template <typename T>
struct NoSwapConcept {
  static constexpr bool kValue =
      !HasUserSwap<T>::kValue && !HasStdSwap<T>::kValue;
};

template <typename T>
struct DeferableRunner {
  explicit DeferableRunner(T&& h) : runner(std::move(h)) {}
  ~DeferableRunner() { runner(); }

 private:
  T runner;
};

template <typename T>
struct TransactionRunner {
  explicit TransactionRunner(T&& h) : runner(std::move(h)), rollback(true) {}
  void Commit() { rollback = false; }
  ~TransactionRunner() {
    if (rollback) runner();
  }

 private:
  T runner;
  bool rollback;
};

template <typename T>
DeferableRunner<T> MkDeferableRunner(T&& h) {
  return DeferableRunner{std::move(h)};
}

}  // namespace _

template <typename T, std::enable_if_t<_::HasStdSwap<T>::kValue, int> _ = 0>
T Pass(T* t) noexcept {
  T tmp{};
  tmp.swap(*t);
  return tmp;
}

template <typename T, std::enable_if_t<_::HasUserSwap<T>::kValue, int> _ = 0>
T Pass(T* t) noexcept {
  T tmp{};
  tmp.Swap(t);
  return tmp;
}

template <typename T, std::enable_if_t<_::NoSwapConcept<T>::kValue, int> _ = 0>
T Pass(T* v) noexcept {
  T tmp{};
  std::swap(tmp, *v);
  return tmp;
}

template <typename T>
T Pass(std::optional<T>* v) {
  if (!v) Die("unref empty optional");
  DEFER([&]() { *v = std::nullopt; });
  return T{std::move(v->value())};
}

template <typename T>
T Pass(std::optional<T>& v) {
  if (!v) Die("undef empty optional");
  DEFER([&]() { v = std::nullopt; });
  return T{std::move(*v)};
}

class NonCopyable {
 public:
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;

 protected:
  NonCopyable() = default;
  ~NonCopyable() = default;
};

template <typename T>
void DestoryInplace(T& ref) noexcept {
  ref.~T();
}

template <typename T, typename... ARGS>
void ConstructInplace(T& ref, ARGS&&... args) noexcept {
  ::new (std::addressof(ref)) T(std::forward(args)...);
}

template <typename T>
class ObjectInplace final : NonCopyable {
 public:
  ObjectInplace() = default;
  ~ObjectInplace() noexcept {
    if (initialize_) {
      DestoryInplace(*ptr());
    }
  }

  ObjectInplace& operator=(T&& value) noexcept {
    ConstructInplace(*ptr(), std::move(value));
    initialize_ = true;
    return *this;
  }

  ObjectInplace& operator=(const T& value) noexcept {
    DCHECK(!initialize_);
    ConstructInplace(*ptr(), value);
    initialize_ = true;
    return *this;
  }

 public:
  T& operator*() noexcept {
    DCHECK(initialize_);
    return *ptr();
  }

  const T& operator*() const noexcept {
    DCHECK(initialize_);
    return *ptr();
  }

 private:
  T* ptr() noexcept { return reinterpret_cast<T*>(&data_); }
  const T* ptr() const noexcept { return reinterpret_cast<const T*>(&data_); }

 private:
  std::aligned_storage<sizeof(T), alignof(T)> data_;
  bool initialize_{false};
};

template <typename T>
class ObjectInplace<T&> final : NonCopyable {
 public:
  ObjectInplace() = default;
  ~ObjectInplace() = default;

  ObjectInplace& operator=(T& value) noexcept {
    DCHECK(!initialize_);
    ptr_ = &value;
    initialize_ = true;
    return *this;
  }

 public:
  T& operator*() noexcept {
    DCHECK(initialize_);
    return *ptr_;
  }

  const T& operator*() const noexcept {
    DCHECK(initialize_);
    return *ptr_;
  }

 private:
  T* ptr_{nullptr};
  bool initialize_{false};
};

}  // namespace base

