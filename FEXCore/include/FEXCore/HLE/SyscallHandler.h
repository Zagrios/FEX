// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <optional>

#include <FEXCore/Core/CodeCache.h>
#include <FEXCore/IR/IR.h>
#include <FEXCore/fextl/string.h>

namespace FEXCore::Context {
class Context;
}

namespace FEXCore::Core {
struct InternalThreadState;
struct CpuStateFrame;
} // namespace FEXCore::Core

namespace FEXCore::HLE {
struct ExecutableRangeInfo {
  uint64_t Base;
  uint64_t Size;
  bool Writable;
};

class SyscallHandler;
class SourcecodeResolver;

class SyscallHandler {
public:
  virtual ~SyscallHandler() = default;

  virtual void HandleSyscall(FEXCore::Core::CpuStateFrame* Frame) = 0;

  virtual void MarkGuestExecutableRange(FEXCore::Core::InternalThreadState* Thread, uint64_t Start, uint64_t Length) {}
  // With SMCChecks=mtrack: guest code in [Start, Start + Length) whose writes the frontend does not track. Translations
  // check the bytes of each instruction there before running it, as SMCChecks=full does for all code. Asked while
  // compiling (code invalidation mutex held shared): a frontend that starts answering true for a range must
  // invalidate the range afterwards.
  virtual bool NeedsCodeValidation(uint64_t Start, uint64_t Length) {
    return false;
  }
  virtual void InvalidateGuestCodeRange(FEXCore::Core::InternalThreadState* Thread, uint64_t Start, uint64_t Length) {}
  virtual void MarkOvercommitRange(uint64_t Start, uint64_t Length) {}
  virtual void UnmarkOvercommitRange(uint64_t Start, uint64_t Length) {}
  virtual ExecutableRangeInfo QueryGuestExecutableRange(FEXCore::Core::InternalThreadState* Thread, uint64_t Address) = 0;
  virtual std::optional<ExecutableFileSectionInfo> LookupExecutableFileSection(Core::InternalThreadState* Thread, uint64_t GuestAddr) = 0;

  // Darwin port: whether the guest bytes of [Start, Start + Length) can be read without opening a guarded host page
  // (ReviveTranslations hashes them only then).
  virtual bool CanReadGuestCode(uint64_t Start, uint64_t Length) {
    return true;
  }

  virtual void PreCompile() {}
  // After every block compilation (the decoder has read the guest's code), on every path out of it.
  virtual void PostCompile() {}

  virtual SourcecodeResolver* GetSourcecodeResolver() {
    return nullptr;
  }

  virtual void SleepThread(FEXCore::Context::Context* CTX, FEXCore::Core::CpuStateFrame* Frame) {}
};
} // namespace FEXCore::HLE
