#include <common/error.h>

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

  return 0;
}
