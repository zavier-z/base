#pragma once

#include <functional>

namespace base {
namespace event {

// |Executor| provides the execution environment for function/callback. the
// underlaying implementation maybe athread pool and so on
class Executor {
 public:
  virtual ~Executor() {}

  // run a function/callback on appropriate time
  virtual void Post(std::function<void()>&&) = 0;
};

class LocalExecutor : public Executor {
 private:
  // the function/callback is called in place
  void Post(std::function<void()>&& f) override { f(); }
};

}  // namespace event
}  // namespace base
