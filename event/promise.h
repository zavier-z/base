#pragma once

#include <common/bind.h>
#include <common/common.h>
#include <common/error.h>
#include <common/result.h>
#include <common/trait.h>

#include <functional>
#include <memory>
#include <iostream>

#include "basic.h"
#include "executor.h"

namespace base {
namespace event {
template <typename T>
class Promise;

template <typename T>
struct IsPromise : std::false_type {};

template <typename T>
struct IsPromise<Promise<T>> : std::true_type {};

namespace _ {

enum class PromiseStatus : std::uint8_t {
  // initial state
  kInit,
  // pre-fulfilled, pending state, its callback will be invoked
  kPreFulfilled,
  // fulfilled, its callback has already been invoked in executor
  kFulfilled,
  // pre-rejected state, pending state. its callback will be invoked
  kPreRejected,
  // rejected, its callback has already been invoked in executor
  kRejected,
  // cancalled, its callback and storage has been purged
  kCancelled,
};

class PromiseStatusMachine {
 public:
  PromiseStatusMachine() = default;

  PromiseStatus status() const { return status_; }

  bool IsEmpty() const { return status() == PromiseStatus::kInit; }
  bool IsPreFulfilled() const {
    return status() == PromiseStatus::kPreFulfilled;
  }
  bool IsFulfilled() const { return status() == PromiseStatus::kFulfilled; }
  bool IsPreRejected() const { return status() == PromiseStatus::kPreRejected; }
  bool IsRejected() const { return status() == PromiseStatus::kRejected; }
  bool IsCancelled() const { return status() == PromiseStatus::kCancelled; }

 public:
  bool ToPreFulfilled() {
    return To(PromiseStatus::kInit, PromiseStatus::kPreFulfilled);
  }
  bool ToFulfilled() {
    return To(PromiseStatus::kPreFulfilled, PromiseStatus::kFulfilled);
  }
  bool ToPreRejected() {
    return To(PromiseStatus::kInit, PromiseStatus::kPreRejected);
  }
  bool ToRejected() {
    return To(PromiseStatus::kPreRejected, PromiseStatus::kRejected);
  }
  bool ToCancelled() {
    switch (status()) {
      case PromiseStatus::kInit:
      case PromiseStatus::kPreRejected:
      case PromiseStatus::kPreFulfilled:
        status_ = PromiseStatus::kCancelled;
        return true;
      default:
        return false;
    }
  }

  void Force(PromiseStatus s) { status_ = s; }

 public:
  // the callback has not been invoked
  bool IsPending() const { return IsPreRejected() || IsPreFulfilled(); }

  // the callback has been invoked in given executor
  bool IsDone() const { return IsFulfilled() || IsRejected(); }

  // the promise has invoked |resolve|
  bool IsSatisfied() const { return IsPreFulfilled() || IsFulfilled(); }

  // the promise has invoked |reject|
  bool IsUnsatisfied() const { return IsPreRejected() || IsRejected(); }

  // the result has been settled
  bool IsSettled() const { return !IsEmpty() && !IsCancelled(); }

 private:
  bool To(PromiseStatus from, PromiseStatus to) {
    if (status() == from) {
      status_ = to;
      return true;
    }
    return false;
  }

  PromiseStatus status_{PromiseStatus::kInit};
};

template <typename T>
class PromiseState : public std::enable_shared_from_this<PromiseState<T>> {
 public:
  using ValueType = T;
  using Callback = std::function<void(base::Result<T>&&)>;

  PromiseState() : status_(), storage_(), callback_(), executor_(nullptr) {}

  PromiseState(PromiseState&&) = default;
  PromiseState& operator=(PromiseState&&) = default;

 public:
  template <typename U>
  bool Resolve(U&& value) {
    if (IsEmpty()) {
      storage_.emplace(std::forward<U>(value));
      status_.ToPreFulfilled();
      TryInvokeCallback();
      return true;
    }
    return false;
  }

  bool Reject(base::Error&& e) {
    if (IsEmpty()) {
      storage_.emplace(std::move(e));
      status_.ToPreRejected();
      TryInvokeCallback();
      return true;
    }
    return false;
  }

  void Cancel() {
    if (IsEmpty() || IsPending()) {
      callback_ = {};
      storage_ = std::nullopt;
      status_.ToCancelled();
    }
  }

 public:
  PromiseStatus status() const { return status_.status(); }

  bool IsDone() const { return status_.IsDone(); }
  bool IsPending() const { return status_.IsPending(); }
  bool IsSatisfied() const { return status_.IsSatisfied(); }
  bool IsUnsatisfied() const { return status_.IsUnsatisfied(); }
  bool IsSettled() const { return status_.IsSettled(); }

  bool IsEmpty() const { return status_.IsEmpty(); }
  bool IsPreFulfilled() const { return status_.IsPreFulfilled(); }
  bool IsFulfilled() const { return status_.IsFulfilled(); }
  bool IsPreRejected() const { return status_.IsPreRejected(); }
  bool IsRejected() const { return status_.IsRejected(); }
  bool IsCancelled() const { return status_.IsCancelled(); }

 public:
  // function<void(base::Result<T>&&)>
  template <typename F, typename RT = std::invoke_result_t<F, T>,
            std::enable_if_t<std::is_void<RT>::value, int> = 0>
  void Attach(F&& callback, Executor* executor);

  // function<Result<RT>(base::Result<T>&&)>
  template <typename U, typename F, typename RT = std::invoke_result_t<F, T>,
            std::enable_if_t<base::IsResult<RT>::value, int> = 0>
  void Attach(Promise<U>& next, F&& callback, Executor* exectuor);

 private:
  void TryInvokeCallback() {
    if (callback_ && IsPending()) {
      auto cb = [this]() -> void {
        switch (status()) {
          case PromiseStatus::kPreFulfilled:
            std::cout << "prefulfilled" << std::endl;
            CHECK(status_.ToFulfilled());
            InvokeCallback();
            break;

          case PromiseStatus::kPreRejected:
            std::cout << "rejected" << std::endl;
            CHECK(status_.ToRejected());
            InvokeCallback();
            break;

          default:
            std::cout << "default" << std::endl;
            break;
        }
      };

      RunInExecutor(BindWeak(this, std::move(cb)));
    }
  }

  void InvokeCallback() {
    DCHECK(storage_);
    auto tmp = base::Pass(&callback_);
    NO_EXCEPT(tmp(std::move(storage_.value())));
  }

  template <typename F>
  void RunInExecutor(F&& callback) {
    if (executor_) {
      executor_->Post(std::move(callback));
    } else {
      NO_EXCEPT(callback());
    }
  }

  void AddCallback(Callback&& cb, Executor* executor) {
    callback_ = std::move(cb);
    executor_ = executor;

    TryInvokeCallback();
  }

 private:
  PromiseStatusMachine status_;
  std::optional<base::Result<T>> storage_;

  Callback callback_;
  Executor* executor_;

  template <typename U>
  friend class Promise;

  DISALLOW_COPY_AND_ASSIGN(PromiseState);
};

// eg.
// Promise<int> p0;
// p0.Then([&](Result<int>&& r) -> void {
//    return;
// }, &executor);
template <typename T>
template <typename F, typename RT,
          std::enable_if_t<std::is_void<RT>::value, int>>
void PromiseState<T>::Attach(F&& callback, Executor* executor) {
  AddCallback(std::move(callback), executor);
}

// eg.
// Promise<int> p0;
// auto p1 = p0.Then([&](Result<int>&& r) -> Result<bool> {
//    return false;
// }, &exectuor);
template <typename T>
template <typename U, typename F, typename RT,
          std::enable_if_t<base::IsResult<RT>::value, int>>
void PromiseState<T>::Attach(Promise<U>& next, F&& callback,
                             Executor* executor) {
  auto cb = [n = next,
             f = std::forward<F>(callback)](base::Result<T>&& r) mutable {
    auto result = std::invoke(std::forward<decltype(f)>(f), std::move(r));
    n.Propagate(&result);
  };

  AddCallback(std::move(cb), executor);
}

template <>
class PromiseState<void> {
 public:
  PromiseState() : status_(), storage_() {}

  PromiseState(PromiseState&&) = default;
  PromiseState& operator=(PromiseState&&) = default;

  // Since this specialization doesn't need invoke callback in given executor,
  // it's not necessary to transfer state to |kPreFulfilled| and |kPreRejected|.
  // we should force state transition when call |Resolve| and |Reject|.
  bool Resolve() {
    if (IsEmpty()) {
      storage_.emplace(base::Result<void>{});
      status_.Force(PromiseStatus::kFulfilled);
      return true;
    }
    return false;
  }

  bool Reject(base::Error&& e) {
    if (IsEmpty()) {
      storage_.emplace(base::Result<void>{std::move(e)});
      status_.Force(PromiseStatus::kRejected);
      return true;
    }
    return false;
  }

  void Cancel() {
    if (IsEmpty() || IsPending()) {
      storage_ = std::nullopt;
      status_.ToCancelled();
    }
  }

 public:
  PromiseStatus status() const { return status_.status(); }

  bool IsDone() const { return status_.IsDone(); }
  bool IsPending() const { return status_.IsPending(); }
  bool IsSatisfied() const { return status_.IsSatisfied(); }
  bool IsUnsatisfied() const { return status_.IsUnsatisfied(); }
  bool IsSettled() const { return status_.IsSettled(); }

  bool IsEmpty() const { return status_.IsEmpty(); }
  bool IsPreFulfilled() const { return status_.IsPreFulfilled(); }
  bool IsFulfilled() const { return status_.IsFulfilled(); }
  bool IsPreRejected() const { return status_.IsPreRejected(); }
  bool IsRejected() const { return status_.IsRejected(); }
  bool IsCancelled() const { return status_.IsCancelled(); }

 private:
  PromiseStatusMachine status_;
  std::optional<base::Result<void>> storage_;

  template <typename U>
  friend class Promise;

  DISALLOW_COPY_AND_ASSIGN(PromiseState);
};

}  // namespace _

template <typename T>
class PromiseResolver {
 public:
  template <typename U>
  bool Resolve(U&&);

  bool Reject(base::Error&&);
  void Cancel();

  void Reset() { ptr_.reset(); }

 public:
  // check whether the promise's callback has been invoked
  std::optional<bool> IsDone() const;

  // check whether the promise has been initialized
  std::optional<bool> IsEmpty() const;

  // check whether the result has been settled, maybe not invoke callback
  std::optional<bool> IsSettled() const;

  // check the promise has invoked |Resolve|
  std::optional<bool> IsSatisfied() const;

  // check the promise has invoked |Reject|
  std::optional<bool> IsUnsatisfied() const;

  bool IsExpired() const { return ptr_.expired(); }

  PromiseResolver() : ptr_() {}

 protected:
  explicit PromiseResolver(const std::shared_ptr<_::PromiseState<T>>& p)
      : ptr_(p) {}

 private:
  std::weak_ptr<_::PromiseState<T>> ptr_;

  template <typename U>
  friend class Promise;
};

template <typename T>
template <typename U>
bool PromiseResolver<T>::Resolve(U&& val) {
  if (auto p = ptr_.lock(); p) {
    return p->Resolve(std::forward<U>(val));
  }
  return false;
}

template <typename T>
bool PromiseResolver<T>::Reject(base::Error&& e) {
  if (auto p = ptr_.lock(); p) {
    return p->Reject(std::move(e));
  }
  return false;
}

template <typename T>
void PromiseResolver<T>::Cancel() {
  if (auto p = ptr_.lock(); p) {
    p->Cancel();
  }
}

template <typename T>
std::optional<bool> PromiseResolver<T>::IsDone() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsDone();
  }
  return {};
}

template <typename T>
std::optional<bool> PromiseResolver<T>::IsEmpty() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsEmpty();
  }
  return {};
}

template <typename T>
std::optional<bool> PromiseResolver<T>::IsSettled() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsSettled();
  }
  return {};
}

template <typename T>
std::optional<bool> PromiseResolver<T>::IsUnsatisfied() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsUnsatisfied();
  }
  return {};
}

template <typename T>
std::optional<bool> PromiseResolver<T>::IsSatisfied() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsSatisfied();
  }
  return {};
}

template <>
class PromiseResolver<void> {
 public:
  bool Resolve();
  bool Reject(base::Error&&);
  void Cancel();

  void Reset() { ptr_.reset(); }

 public:
  // check whether the promise's callback has been invoked
  std::optional<bool> IsDone() const;

  // check whether the promise has been initialized
  std::optional<bool> IsEmpty() const;

  // check whether the result has been settled, maybe not invoke callback
  std::optional<bool> IsSettled() const;

  // check the promise has invoked |Resolve|
  std::optional<bool> IsSatisfied() const;

  // check the promise has invoked |Reject|
  std::optional<bool> IsUnsatisfied() const;

  bool IsExpired() const { return ptr_.expired(); }

  PromiseResolver() : ptr_() {}

 protected:
  explicit PromiseResolver(const std::shared_ptr<_::PromiseState<void>>& p)
      : ptr_(p) {}

 private:
  std::weak_ptr<_::PromiseState<void>> ptr_;

  template <typename U>
  friend class Promise;
};

bool PromiseResolver<void>::Resolve() {
  if (auto p = ptr_.lock(); p) {
    return p->Resolve();
  }
  return false;
}

bool PromiseResolver<void>::Reject(base::Error&& e) {
  if (auto p = ptr_.lock(); p) {
    return p->Reject(std::move(e));
  }
  return false;
}

void PromiseResolver<void>::Cancel() {
  if (auto p = ptr_.lock(); p) {
    p->Cancel();
  }
}

std::optional<bool> PromiseResolver<void>::IsDone() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsDone();
  }
  return {};
}

std::optional<bool> PromiseResolver<void>::IsEmpty() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsEmpty();
  }
  return {};
}

std::optional<bool> PromiseResolver<void>::IsSettled() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsSettled();
  }
  return {};
}

std::optional<bool> PromiseResolver<void>::IsSatisfied() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsSatisfied();
  }
  return {};
}

std::optional<bool> PromiseResolver<void>::IsUnsatisfied() const {
  if (auto p = ptr_.lock(); p) {
    return p->IsUnsatisfied();
  }
  return {};
}

template <typename T>
class Promise {
 public:
  using ValueType = T;
  using ResolverType = PromiseResolver<T>;

  Promise() : state_(std::make_shared<_::PromiseState<T>>()) {}

  Promise(Promise&&) = default;
  Promise(const Promise&) = default;
  Promise& operator=(Promise&&) = default;
  Promise& operator=(const Promise&) = default;

  template <typename U>
  bool Resolve(U&& value) {
    return state_->Resolve(std::forward<U>(value));
  }

  bool Reject(base::Error&& e) { return state_->Reject(std::move(e)); }

  void Cancel() { state_->Cancel(); }

  ResolverType GetResolver() { return ResolverType{state_}; }

 public:
  bool IsDone() const { return state_->IsDone(); }
  bool IsPending() const { return state_->IsPending(); }
  bool IsSatisfied() const { return state_->IsSatisfied(); }
  bool IsUnsatisfied() const { return state_->IsUnsatisfied(); }
  bool IsSettled() const { return state_->IsSettled(); }

  bool IsEmpty() const { return state_->IsEmpty(); }
  bool IsPreFulfilled() const { return state_->IsPreFulfilled(); }
  bool IsFulfilled() const { return state_->IsFulfilled(); }
  bool IsPreRejected() const { return state_->IsPreRejected(); }
  bool IsRejected() const { return state_->IsRejected(); }
  bool IsCancelled() const { return state_->IsCancelled(); }

 public:
  // the functor return a Result
  template <typename F, typename RT = std::invoke_result_t<F, T>,
            typename R = std::enable_if_t<base::IsResult<RT>::value,
                                          Promise<typename RT::ValueType>>>
  R Then(F&&, Executor*);

  // the functor return a void
  template <typename F, typename RT = std::invoke_result_t<F, T>,
            std::enable_if_t<std::is_void_v<RT>, int> _ = 0>
  void Then(F&&, Executor*);

 public:
  // the functor return a promise container
  template <typename F>
  auto ThenAll(F&& f, Executor* executor) {
    return Then(
        [f = std::forward<F>(f), executor](base::Result<T>&& r) mutable {
          auto result = std::invoke(std::forward<decltype(f)>(f), std::move(r));
          return MkAllPromise(std::move(result), executor);
        },
        executor);
  }

  template <typename F>
  auto ThenAny(F&& f, Executor* executor) {
    return Then(
        [f = std::forward<F>(f), executor](base::Result<T>&& r) mutable {
          auto result = std::invoke(std::forward<decltype(f)>(f), std::move(r));
          return MkAnyPromise(std::move(result), executor);
        },
        executor);
  }

  template <typename F>
  auto ThenRace(F&& f, Executor* executor) {
    return Then(
        [f = std::forward<F>(f), executor](base::Result<T>&& r) mutable {
          auto result = std::invoke(std::forward<decltype(f)>(f), std::move(r));
          return MkRacePromise(std::move(result), executor);
        },
        executor);
  }

 private:
  template <typename U, typename F>
  void DoThen(Promise<U>& promise, F&& functor, Executor* executor) {
    state_->Attach(promise, std::move(functor), executor);
  }

  template <typename F>
  void DoThen(F&& functor, Executor* executor) {
    state_->Attach(std::move(functor), executor);
  }

  void Propagate(void* r) {
    base::Result<T>* result = reinterpret_cast<base::Result<T>*>(r);
    if (*result) {
      Resolve(result->PassResult());
    } else {
      Reject(result->PassError());
    }
  }

 private:
  std::shared_ptr<_::PromiseState<T>> state_;

  template <typename U>
  friend class _::PromiseState;
};

template <typename T>
template <typename F, typename RT, typename R>
R Promise<T>::Then(F&& functor, Executor* executor) {
  Promise<typename RT::ValueType> next;
  DoThen(next, std::move(functor), executor);
  return next;
}

template <typename T>
template <typename F, typename RT, std::enable_if_t<std::is_void_v<RT>, int>>
void Promise<T>::Then(F&& functor, Executor* executor) {
  DoThen(std::move(functor), executor);
}

template <>
class Promise<void> {
 public:
  using ResolverType = PromiseResolver<void>;

  Promise() : state_(std::make_shared<_::PromiseState<void>>()) {}

  Promise(Promise&&) = default;
  Promise(const Promise&) = default;
  Promise& operator=(Promise&&) = default;
  Promise& operator=(const Promise&) = default;

  bool Resolve() { return state_->Resolve(); }
  bool Reject(base::Error&& e) { return state_->Reject(std::move(e)); }
  void Cancel() { state_->Cancel(); }

  ResolverType GetResolver() { return ResolverType{state_}; }

 public:
  bool IsDone() const { return state_->IsDone(); }
  bool IsPending() const { return state_->IsPending(); }
  bool IsSatisfied() const { return state_->IsSatisfied(); }
  bool IsUnsatisfied() const { return state_->IsUnsatisfied(); }
  bool IsSettled() const { return state_->IsSettled(); }

  bool IsEmpty() const { return state_->IsEmpty(); }
  bool IsPreFulfilled() const { return state_->IsPreFulfilled(); }
  bool IsFulfilled() const { return state_->IsFulfilled(); }
  bool IsPreRejected() const { return state_->IsPreRejected(); }
  bool IsRejected() const { return state_->IsRejected(); }
  bool IsCancelled() const { return state_->IsCancelled(); }

 private:
  void Propagate(void* r) {
    base::Result<void>* result = reinterpret_cast<base::Result<void>*>(r);
    if (*result) {
      Resolve();
    } else {
      Reject(result->PassError());
    }
  }

  std::shared_ptr<_::PromiseState<void>> state_;

  template <typename U>
  friend class _::PromiseState;
};

template <typename T, typename U = std::remove_reference_t<T>>
Promise<U> MkResolvedPromise(T&& val) {
  Promise<U> p;
  p.Resolve(std::forward<T>(val));
  return p;
}

template <typename T>
Promise<T> MkRejectedPromise(base::Error&& e) {
  Promise<T> p;
  p.Reject(std::move(e));
  return p;
}

template <typename T, typename F>
Promise<T> MkPromise(F&& f) {
  Promise<T> p;

  auto resolver = [p](T&& v) mutable { return p.Resolve(std::forward<T>(v)); };

  auto rejector = [p](base::Error&& e) mutable {
    return p.Reject(std::move(e));
  };

  std::invoke(std::forward<F>(f), std::move(resolver), std::move(rejector));

  return p;
}

template <typename Itr,
          typename TraitType = typename std::iterator_traits<Itr>::value_type,
          typename ValueType = typename TraitType::value_type,
          typename R = std::vector<ValueType>>
Promise<R> MkAllPromise(Itr begin, Itr end, Executor* executor) {
  if (begin == end) {
    return MkResolvedPromise(R{});
  }

  struct Context {
    std::size_t success_counter;
    std::vector<base::Result<ValueType>> results;
    Context(std::size_t c) : success_counter(c), results(c) {}
  };

  return MkPromise<R>([begin, end, executor](auto&& resolver, auto&& rejector) {
    std::size_t idx = 0;
    auto context = std::make_shared<Context>(std::distance(begin, end));
    for (auto itr = begin; itr != end; ++itr, ++idx) {
      itr->Then(
          [context, resolver, rejector,
           idx](base::Result<ValueType>&& r) mutable {
            if (!r) {
              rejector(r.PassError());
              return;
            }

            context->results[idx] = std::move(r);

            context->success_counter--;
            if (context->success_counter == 0) {
              resolver(std::move(context->results));
            }
          },
          executor);
    }
  });
}

template <typename Cntr>
auto MkAllPromise(Cntr&& container, Executor* executor) {
  return MkAllPromise(std::begin(container), std::end(container), executor);
}

template <typename Itr,
          typename TraitType = typename std::iterator_traits<Itr>::value_type,
          typename ValueType = typename TraitType::value_type,
          typename R = ValueType>
Promise<R> MkAnyPromise(Itr begin, Itr end, Executor* executor) {
  if (begin == end) {
    return MkRejectedPromise<R>(Err(kErrorEventPromiseAny, "no promise"));
  }

  struct Context {
    std::size_t failure_counter;
    std::vector<base::Error> errors;
    Context(std::size_t c) : failure_counter(c), errors(c) {}
  };

  return MkPromise<R>([begin, end, executor](auto&& resolver, auto&& rejector) {
    std::size_t idx = 0;
    auto context = std::make_shared<Context>(std::distance(begin, end));
    for (auto itr = begin; itr != end; ++itr, ++idx) {
      itr->Then(
          [context, resolver, rejector,
           idx](base::Result<ValueType>&& r) mutable {
            if (r) {
              resolver(r.PassResult());
              return;
            }

            context->errors[idx] = r.PassError();
            context->failure_counter--;
            if (context->failure_counter == 0) {
              rejector(Err(kErrorEventPromiseAny, "no resolved promise"));
            }
          },
          executor);
    }
  });
}

template <typename Cntr>
auto MkAnyPromise(Cntr&& container, Executor* executor) {
  return MkAnyPromise(std::begin(container), std::end(container), executor);
}

template <typename Itr,
          typename TraitType = typename std::iterator_traits<Itr>::value_type,
          typename ValueType = typename TraitType::value_type,
          typename R = ValueType>
Promise<R> MkRacePromise(Itr begin, Itr end, Executor* executor) {
  return MkPromise<R>([begin, end, executor](auto&& resolver, auto&& rejector) {
    for (auto itr = begin; itr != end; ++itr) {
      itr->Then(
          [resolver, rejector](base::Result<ValueType>&& r) {
            if (r) {
              resolver(r.PassResult());
            } else {
              rejector(r.PassError());
            }
          },
          executor);
    }
  });
}

template <typename Cntr>
auto MkRacePromise(Cntr&& container, Executor* executor) {
  return MkRacePromise(std::begin(container), std::end(container), executor);
}

}  // namespace event
}  // namespace base
