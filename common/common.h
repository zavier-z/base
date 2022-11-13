#pragma once

#include <cstdarg>
#include <cstdlib>
#include <exception>
#include <string>

#include "check.h"

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

}  // namespace _
}  // namespace base

