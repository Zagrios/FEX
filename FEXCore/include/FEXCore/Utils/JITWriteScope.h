// SPDX-License-Identifier: MIT
#pragma once

// Darwin port. On Apple Silicon a MAP_JIT region cannot be writable and executable for the same thread at the same
// time. pthread_jit_write_protect_np() switches the calling thread only: other threads keep executing the region
// while this thread patches it. JITWriteScope is the RAII form of that switch, with a per-thread nesting counter so a
// scope may be opened inside another one (for example a signal handler that interrupts an emission).
//
// Rules for users of this header:
//  - Never execute code from a MAP_JIT region while a JITWriteScope is open on the same thread.
//  - Keep the scope as short as the stores it protects.
// On every other host the class is empty.

#ifdef __APPLE__
#include <pthread.h>
#include <libkern/OSCacheControl.h>
#endif

namespace FEXCore::Utils {
#ifdef __APPLE__
inline thread_local int JITWriteScopeDepth = 0;

class JITWriteScope final {
public:
  JITWriteScope() {
    if (JITWriteScopeDepth++ == 0) {
      pthread_jit_write_protect_np(0);
    }
  }
  ~JITWriteScope() {
    if (--JITWriteScopeDepth == 0) {
      pthread_jit_write_protect_np(1);
    }
  }
  JITWriteScope(const JITWriteScope&) = delete;
  JITWriteScope& operator=(const JITWriteScope&) = delete;

  // True when the calling thread is inside a scope (its JIT regions are writable and not executable).
  static bool IsOpen() {
    return JITWriteScopeDepth != 0;
  }
};
#else
class JITWriteScope final {
public:
  JITWriteScope() {}
  ~JITWriteScope() {}
  JITWriteScope(const JITWriteScope&) = delete;
  JITWriteScope& operator=(const JITWriteScope&) = delete;
  static bool IsOpen() {
    return false;
  }
};
#endif
} // namespace FEXCore::Utils
