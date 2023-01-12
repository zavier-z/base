#define CATCH_CONFIG_MAIN
#include "promise.h"

#include <catch-include.h>

#include <list>

#include "executor.h"

namespace base {
namespace event {

struct MockExecutor : public Executor {
  using Callback = std::function<void()>;
  std::size_t count{0};
  std::list<Callback> queue;

  void Post(Callback&& f) { queue.push_back(std::move(f)); }

  void Run() {
    while (!queue.empty()) {
      auto cb = std::move(queue.front());
      queue.pop_front();
      cb();
      ++count;
    }
  }

  bool RunOne() {
    if (!queue.empty()) {
      auto cb = std::move(queue.front());
      queue.pop_front();
      cb();
      ++count;
      return true;
    }
    return false;
  }
};

TEST_CASE("basic", "[promise]") {
  {
    MockExecutor exec;

    int value = 0;
    Promise<int> p1;

    REQUIRE(p1.IsEmpty());

    Promise<void> p2 = p1.Then(
        [&](Result<int>&& v) -> Result<void> {
          value = v.GetResult();
          return {};
        },
        &exec);

    REQUIRE(p1.IsEmpty());
    REQUIRE(p2.IsEmpty());

    REQUIRE(exec.queue.size() == 0);

    REQUIRE(p1.GetResolver().Resolve(2022));

    REQUIRE(exec.queue.size() == 1);
    REQUIRE(p1.IsPending());
    REQUIRE(p2.IsEmpty());

    exec.Run();

    REQUIRE(value == 2022);
    REQUIRE(p1.IsFulfilled());
    REQUIRE(p2.IsFulfilled());
  }
}

}  // namespace event
}  // namespace base
