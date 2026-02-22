// -*-C++-*-

#pragma once

#include <concepts>

#include <functional>
#include <utility>

#include <unistd.h>

template<typename T> concept has_no_cv = std::same_as<T, std::remove_cv_t<T>>;

template<typename T, typename... Ts>
concept is_one_of = (std::same_as<T, Ts> || ...);

// Note that Destroy generally should not throw, whether or not it is
// declared noexcept.  Only an explicit call to reset() will allow
// exceptions to propagate.
template<typename T, auto Empty, auto Destroy> struct RaiiHelper {
  T t_ = Empty;

  RaiiHelper() noexcept = default;
  RaiiHelper(T t) noexcept : t_(std::move(t)) {}
  RaiiHelper(RaiiHelper &&other) noexcept : t_(other.release()) {}
  ~RaiiHelper() { reset(); }

  template<is_one_of<T, decltype(Empty)> Arg>
  RaiiHelper &operator=(Arg &&arg) noexcept
  {
    reset(std::forward<Arg>(arg));
    return *this;
  }
  RaiiHelper &operator=(RaiiHelper &&other) noexcept
  {
    return *this = other.release();
  }

  explicit operator bool() const noexcept { return t_ != Empty; }
  const T &operator*() const noexcept { return t_; }

  T release() noexcept { return std::exchange(t_, Empty); }

  template<is_one_of<T, decltype(Empty)> Arg>
  void reset(Arg &&arg) noexcept(noexcept(Destroy(std::move(t_))))
  {
    if (auto destroy_me = std::exchange(t_, std::forward<Arg>(arg));
        destroy_me != Empty)
      Destroy(std::move(destroy_me));
  }
  void reset() noexcept(noexcept(reset(Empty))) { reset(Empty); }
};

// Self-closing file descriptor
using Fd = RaiiHelper<int, -1, ::close>;

namespace detail {
inline void
defer_helper(std::move_only_function<void()> &&f) noexcept
{
  f();
}
} // namespace detail
// Deferred cleanup action
using Defer =
    RaiiHelper<std::move_only_function<void()>, nullptr, detail::defer_helper>;
