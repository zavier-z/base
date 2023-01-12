#include "basic.h"

#include <common/error.h>
#include <fmt/format.h>

namespace base {
namespace event {
namespace {

struct EventCategory : public Error::Category {
  const char* GetName() const override { return "event"; }
  std::string GetInformation(std::uint32_t c) const override {
    switch (c) {
#define __(A, B) \
  case A:        \
    return fmt::format("event[{}]", B);
      EVENT_ERROR_LIST(__)
#undef __
      default:
        return "event[none]";
    }
  }
};

}  // namespace

const Error::Category* Cat() {
  static EventCategory kC;
  return &kC;
}

}  // namespace event
}  // namespace base
