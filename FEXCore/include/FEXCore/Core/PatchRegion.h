// SPDX-License-Identifier: MIT
// Darwin port: once a frontend sets the hooks, the end of every 128 MiB zone of a JIT code buffer is kept free of code for
// stubs it patches translated code to (fexdarwin GuardedLoadPatch). A branch reaches +-128 MiB, code never crosses the
// end of its zone (CodeBuffer::AtomicAllocateBuffer), and the address space around a code buffer is often full: inside
// its zone, every instruction reaches the stubs. Allocated and Freed are called once per zone.
#pragma once

#include <cstddef>
#include <cstdint>

namespace FEXCore::CPU {
struct PatchRegionHooks {
  // A code zone [CodeBegin, RegionBegin) was allocated with its patch region [RegionBegin, RegionBegin + RegionSize).
  void (*Allocated)(uint64_t CodeBegin, uint64_t RegionBegin, size_t RegionSize);
  // The buffer is about to be freed: no thread runs its code any more.
  void (*Freed)(uint64_t RegionBegin);
};
// Set by the frontend before the first code buffer, both or neither.
inline PatchRegionHooks PatchRegion {};
} // namespace FEXCore::CPU
