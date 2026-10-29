// SPDX-License-Identifier: MIT
// Darwin port: the guest code invalidations asked for, for ReviveTranslations. A frontend notes each one before it
// waits for the code invalidation lock, so a compilation that holds the lock shared can tell whether one may have
// changed the guest bytes it decoded: it then keeps no record of them. The last RingSize requests are kept with their
// range; a compilation that saw more requests than that keeps no record either.
#pragma once

#include <atomic>
#include <cstdint>

namespace FEXCore::Context {
struct CodeInvalidationLog {
  static constexpr uint64_t RingSize = 256;
  struct Entry {
    std::atomic<uint64_t> Seq {0}; // request number + 1 once Start and End are written
    std::atomic<uint64_t> Start {0}, End {0};
  };
  std::atomic<uint64_t> Requests {0};
  Entry Ring[RingSize];
  // Statistics: compiled units recorded, not recorded because an invalidation may have touched their bytes, revived.
  std::atomic<uint64_t> UnitsRecorded {0}, UnitsSkipped {0}, UnitsRevived {0};

  void Note(uint64_t Start, uint64_t Length) {
    const uint64_t N = Requests.fetch_add(1, std::memory_order_acq_rel);
    Entry& E = Ring[N % RingSize];
    E.Seq.store(0, std::memory_order_relaxed);
    E.Start.store(Start, std::memory_order_relaxed);
    E.End.store(Start + Length, std::memory_order_relaxed);
    E.Seq.store(N + 1, std::memory_order_release);
  }

  // True when a request numbered in [From, Requests) may overlap [Start, End): it overlaps, it is not readable yet, or it
  // fell out of the ring.
  bool MayHaveTouched(uint64_t From, uint64_t Start, uint64_t End) const {
    const uint64_t To = Requests.load(std::memory_order_acquire);
    if (To - From > RingSize) {
      return true;
    }
    for (uint64_t N = From; N < To; N++) {
      const Entry& E = Ring[N % RingSize];
      if (E.Seq.load(std::memory_order_acquire) != N + 1) {
        return true;
      }
      const uint64_t S = E.Start.load(std::memory_order_relaxed), X = E.End.load(std::memory_order_relaxed);
      if (E.Seq.load(std::memory_order_acquire) != N + 1 || (S < End && Start < X)) {
        return true;
      }
    }
    return false;
  }
};
inline CodeInvalidationLog CodeInvalidations {};
} // namespace FEXCore::Context
