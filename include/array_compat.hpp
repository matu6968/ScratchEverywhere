#pragma once
#include <array>
#include <type_traits>

// libc++ only got std::array's deduction guides in version 7 (LLVM 7), which is newer than the oldest NDK we build with
// (r16b, needed for ARMv6 devices), so `constexpr std::array x = {...};` doesn't compile there without this.
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 7000
_LIBCPP_BEGIN_NAMESPACE_STD
template <class T, class... U>
array(T, U...) -> array<enable_if_t<(is_same_v<T, U> && ...), T>, 1 + sizeof...(U)>;
_LIBCPP_END_NAMESPACE_STD
#endif
