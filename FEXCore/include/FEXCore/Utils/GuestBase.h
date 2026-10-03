// SPDX-License-Identifier: MIT
// Darwin port: guest base offset. The host address of guest address g is GuestBase + g.
// An arm64 macOS process cannot map below 4 GiB, so guests that need low addresses (Wine: 0x7ffe0000)
// run inside one reserved host window starting at GuestBase.
#pragma once
#include <cstdint>

namespace FEXCore {
#ifdef __APPLE__
#define FEX_GUEST_BASE 1
inline uint64_t GuestBase = 0;
inline void* G2H(uint64_t Guest) {
  return reinterpret_cast<void*>(GuestBase + Guest);
}
inline uint64_t H2G(const void* Host) {
  return reinterpret_cast<uint64_t>(Host) - GuestBase;
}
#else
inline void* G2H(uint64_t Guest) {
  return reinterpret_cast<void*>(Guest);
}
inline uint64_t H2G(const void* Host) {
  return reinterpret_cast<uint64_t>(Host);
}
#endif
} // namespace FEXCore
