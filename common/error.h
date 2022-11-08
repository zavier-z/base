#pragma once

#include <optional>
#include <string>

namespace base {

class Error {
 public:
  class Category {
   public:
    virtual const char* GetName() const = 0;
    virtual std::string GetInformation(std::uint32_t) const = 0;

    virtual ~Category() {}
  };

  static constexpr std::uint32_t kNoErrorCode = 0;

 public:
  Error(const Category* category, std::uint32_t code)
      : code_(code), category_(category), message_() {}
  Error(const Category* category, std::uint32_t code, const std::string& msg)
      : code_(code), category_(category), message_(msg) {}
  Error(const Category* category, std::uint32_t code, std::string&& msg)
      : code_(code), category_(category), message_(std::move(msg)) {}

 public:
  std::uint32_t code() const { return code_; }
  const Category* category() const { return category_; }
  std::string information() const { return category_->GetInformation(code_); }

  bool Has() const { return category_ != nullptr; }
  operator bool() const { return Has(); }
  bool HasMessage() const { return message_.has_value(); }
  // TODO: maybe core
  std::string GetMessage() const { return message_.value(); }
  std::string PassMessage() { return std::move(*message_); }

  void Clear() {
    category_ = nullptr;
    message_.reset();
    code_ = kNoErrorCode;
  }

 private:
  std::uint32_t code_;
  const Category* category_;
  std::optional<std::string> message_;
};

}  // namespace base

