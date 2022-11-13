#include "common.h"

#include <absl/debugging/stacktrace.h>
#include <absl/debugging/symbolize.h>
#include <iostream>
namespace base {

std::string GetStackTrace() {
  static constexpr int kMaxStackDepth = 100;

  int sizes[kMaxStackDepth];
  void* result[kMaxStackDepth];
  int depth = absl::GetStackFrames(result, sizes, kMaxStackDepth, 1);

  std::string bt;
  for (auto i = 0; i < depth; ++i) {
    char tmp[1024];
    if(absl::Symbolize(result[i], tmp, sizeof(tmp))) {
      bt.append(tmp);
    }
  }

  return bt;
}


}  // namespace base

