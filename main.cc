#include <common/error.h>
#include <common/common.h>
#include <iostream>

namespace base {
class CustomErrorCategory : public Error::Category {
 public:
  const char* GetName() const override { return "custom"; }

  std::string GetInformation(std::uint32_t code) const override {
    return "[custom] null";
  }
};

}  // namespace base

const base::Error::Category* Cat() {
  static const base::CustomErrorCategory kC;
  return &kC;
}

int main(int argc, char* argv[]) {
  base::Error err(Cat(), 0);

  std::cout << base::GetStackTrace() << std::endl;

  return 0;
}
