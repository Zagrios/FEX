// SPDX-License-Identifier: MIT
/*
$info$
category: code buffer ~ Thread shared code buffer management
tags: backend|shared
$end_info$
*/
#pragma once

#include <FEXCore/Core/PatchRegion.h>
#include <FEXCore/fextl/memory.h>
#include <FEXCore/Utils/MathUtils.h>
#include <FEXCore/Utils/SignalScopeGuards.h>
#include <FEXCore/Utils/TypeDefines.h>

#include <cstddef>
#include <cstdint>

namespace FEXCore {
struct GuestToHostMap;
}

namespace FEXCore::CPU {
struct CodeBuffer {
  fextl::unique_ptr<GuestToHostMap> LookupCache;

  CodeBuffer(size_t Size, bool ShouldBeNamed);
  CodeBuffer(const CodeBuffer&) = delete;
  CodeBuffer& operator=(const CodeBuffer&) = delete;
  CodeBuffer(CodeBuffer&& oth) = delete;
  CodeBuffer& operator=(CodeBuffer&&) = delete;

  ~CodeBuffer();

  // Atomically allocate a fixed size buffer out of the current allocated codebuffer.
  // Lockless because it's just a linear allocator.
  struct CodeBufferAllocation {
    const uint8_t* BufferBase;
    uint8_t* BufferAllocationOffset;
  };

  CodeBufferAllocation AtomicAllocateBuffer(size_t Size) {
    Size = FEXCore::AlignUp(Size, 16);
    LOGMAN_THROW_A_FMT(reinterpret_cast<uintptr_t>(CodeBufferOffset.load()) % 16 == 0, "Buffer needs to always be 16B aligned!");

    auto ExpectedOffset = CodeBufferOffset.load(std::memory_order_relaxed);
    for (;;) {
      // An allocation never crosses the end of the code of its zone: it starts the next zone instead.
      uint8_t* Start = ExpectedOffset;
      const size_t Zone = static_cast<size_t>(Start - Ptr) / ZoneBytes();
      if (Zone >= NumZones()) {
        return {};
      }
      if (Start + Size > ZoneCodeEnd(Zone)) {
        if (Zone + 1 >= NumZones() || ZoneBegin(Zone + 1) + Size > ZoneCodeEnd(Zone + 1)) {
          // Couldn't fit.
          return {};
        }
        Start = ZoneBegin(Zone + 1);
      }
      if (CodeBufferOffset.compare_exchange_strong(ExpectedOffset, Start + Size)) {
        // Managed to fit.
        return {
          .BufferBase = Ptr,
          .BufferAllocationOffset = Start,
        };
      }
    }
  }

  // Returns the total number of bytes available for storing code
  size_t UsableSize() const {
    return AllocatedSize - FEXCore::Utils::FEX_HOST_PAGE_SIZE - NumZones() * PatchRegionSize();
  }

  // Darwin port: with patch regions (FEXCore/Core/PatchRegion.h), a buffer larger than ZoneSize is cut into zones of
  // ZoneSize, each ending with its own patch region, so that every instruction reaches the stubs of its zone with a
  // branch (+-128 MiB). Without them, or up to ZoneSize, the buffer is one zone.
  static constexpr size_t ZoneSize = 128 * 1024 * 1024;
  size_t NumZones() const {
    return FEXCore::CPU::PatchRegion.Allocated && AllocatedSize > ZoneSize ? AllocatedSize / ZoneSize : 1;
  }
  size_t ZoneBytes() const {
    return NumZones() == 1 ? AllocatedSize : ZoneSize;
  }
  uint8_t* ZoneBegin(size_t Zone) const {
    return Ptr + Zone * ZoneBytes();
  }
  // The end of the code of a zone: its patch region starts there (and the guard page follows the last one).
  uint8_t* ZoneCodeEnd(size_t Zone) const {
    return ZoneBegin(Zone) + ZoneBytes() - PatchRegionSize() - (Zone + 1 == NumZones() ? FEXCore::Utils::FEX_HOST_PAGE_SIZE : 0);
  }
  // Whether Address is in translated code: in a zone, outside its patch region.
  bool ContainsCode(uintptr_t Address) const {
    const auto Base = reinterpret_cast<uintptr_t>(Ptr);
    if (Address < Base || Address >= Base + AllocatedSize) {
      return false;
    }
    const size_t Zone = (Address - Base) / ZoneBytes();
    return Zone < NumZones() && Address < reinterpret_cast<uintptr_t>(ZoneCodeEnd(Zone));
  }

  // The bytes at the end of each zone kept for patch stubs, 1/16 of the zone, when the frontend asked for them
  // (FEXCore/Core/PatchRegion.h).
  size_t PatchRegionSize() const {
    return FEXCore::CPU::PatchRegion.Allocated ? FEXCore::AlignDown(ZoneBytes() / 16, FEXCore::Utils::FEX_HOST_PAGE_SIZE) : 0;
  }

  // Returns the full size of the buffer, including the guard page.
  size_t TotalAllocationSize() const {
    return AllocatedSize;
  }

  // Returns the num of bytes currently allocated from the allocator.
  size_t AllocatedSpaceUsed() const {
    return CodeBufferOffset - Ptr;
  }

  // Trivially reset the allocator.
  void Reset() {
    CodeBufferOffset = Ptr;
  }

  // Returns the base of the buffer.
  uint8_t* GetBufferBase() const {
    return Ptr;
  }

private:
  uint8_t* Ptr;
  uint8_t* CodeBufferEnd;
  size_t AllocatedSize; // including guard page; see UsableSize()

  // Code buffer allocation information.
  std::atomic<uint8_t*> CodeBufferOffset {};
};

/**
 * A manager that coordinates access to the CodeBuffer used for compiling new code across threads.
 *
 * The CodeBuffer is managed as a partially persistent data structure:
 * - Exactly one CodeBuffer is now designated as "active", which means data can be appended to it
 * - Lossy modifications to the active CodeBuffer will not invalidate any data in use by other threads (which is what enables save CodeBuffer sharing across threads)
 * - Instead, such lossy modifications trigger a new "version" of the data in the modifying thread. Old versions of the CodeBuffer persist as read-only data for use by the other threads.
 * - The other threads can update their version of the CodeBuffer. This will decrease the reference count and eventually trigger deallocation of the old version
 */
class SharedCodeBufferManager {
public:
  SharedCodeBufferManager();
  virtual ~SharedCodeBufferManager() = default;

  // Get the CodeBuffer that was most recently allocated.
  // This is the only CodeBuffer that data may be written to.
  fextl::shared_ptr<CodeBuffer> GetLatest();

  // Allocate a new CodeBuffer with geometric growth up to an internal maximum.
  // Subsequent calls to GetLatest will point to the returned buffer.
  fextl::shared_ptr<CodeBuffer> StartLargerCodeBuffer();

  // Allocate a new CodeBuffer with maximum internal size.
  // Subsequent calls to GetLatest will point to the returned buffer.
  fextl::shared_ptr<CodeBuffer> StartMaximalCodeBuffer();

  virtual void OnCodeBufferAllocated(const std::shared_ptr<CodeBuffer>&) {};

private:
  fextl::shared_ptr<CodeBuffer> Latest;

  fextl::shared_ptr<CodeBuffer> AllocateNew(size_t Size);

  bool NameJITBuffers {true};
};
} // namespace FEXCore::CPU
