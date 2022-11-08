#pragma once

#include <new>
#include <utility>

namespace base {

// The C++ standard says that destructors for function-level static variables
// are invoked at the termination of the program (eg. after main() exits).This
// behavior sometimes causes very weird and hard to track down issues, so we
// generally try to avoid having static function-level variables with
// complicated destructors
//
// the destructor maybe lead to an extra level of indirection and touching of an
// extra cache line every time.
template <typename T>
class NoDestructor {
 public:
  template <typename... ARGS>
  explicit NoDestructor(ARGS&&... args) {
    new (storage_) T(std::forward<ARGS>(args)...);
  }

  explicit NoDestructor(const T& x) { new (storage_) T(x); }

  explicit NoDestructor(T&& x) { new (storage_) T(std::move(x)); }

  ~NoDestructor() = default;

 public:
  const T& operator*() const { return *get(); }
  T& operator*() { return *get(); }

  const T* operator->() const { return get(); }
  T* operator->() { return get(); }

  const T* get() const { return reinterpret_cast<const T*>(storage_); }
  T* get() { return reinterpret_cast<T*>(storage_); }

 private:
  alignas(T) char storage_[sizeof(T)];
};

}  // namespace base
