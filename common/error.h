#pragma once

#include <optional>
#include <string>

#include "check.h"

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
  Error() : code_(kNoErrorCode), category_(nullptr), message_() {}
  Error(const Category* category, std::uint32_t code)
      : code_(code), category_(category), message_() {}
  Error(const Category* category, std::uint32_t code, const std::string& msg)
      : code_(code), category_(category), message_(msg) {}
  Error(const Category* category, std::uint32_t code, std::string&& msg)
      : code_(code), category_(category), message_(std::move(msg)) {}

  Error(Error&&) = default;
  Error(const Error&) = default;
  Error& operator=(Error&&) = default;
  Error& operator=(const Error&) = default;

 public:
  std::uint32_t code() const { return code_; }
  const Category* category() const { return category_; }
  std::string information() const { return category_->GetInformation(code_); }

  bool Has() const { return category_ != nullptr; }
  operator bool() const { return Has(); }

  bool HasMessage() const { return message_.has_value(); }
  inline std::string PassMessage();
  inline std::string GetMessage() const;

  inline void Clear();

 private:
  std::uint32_t code_;
  const Category* category_;
  std::optional<std::string> message_;
};

inline void Error::Clear() {
  category_ = nullptr;
  message_.reset();
  code_ = kNoErrorCode;
}

inline std::string Error::GetMessage() const {
  DCHECK(HasMessage());
  return message_.value();
}

inline std::string Error::PassMessage() {
  DCHECK(HasMessage());
  return std::move(*message_);
}

}  // namespace base

