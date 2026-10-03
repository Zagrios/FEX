// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>

namespace FEXCore::Utils {
// FEX assumes an operating page size of 4096
// To work around build systems that build on a 16k/64k page size, define our page size here
// Don't use the system provided PAGE_SIZE define because of this.
constexpr size_t FEX_PAGE_SIZE = 4096;
constexpr size_t FEX_PAGE_SHIFT = 12;
constexpr size_t FEX_PAGE_MASK = ~(FEX_PAGE_SIZE - 1);

// Darwin port: FEX_PAGE_SIZE above is the x86 *guest* page size (granularity of the lookup cache page table, of the
// code page tracking and of the frontend page-crossing checks). Memory that FEX itself protects or maps on the host
// (code buffer guard pages, the thread interrupt fault page, JIT temporary buffers) is governed by the *host* page
// size. Apple Silicon macOS uses 16 KiB pages, so mprotect() on a 4 KiB unit fails or protects more than asked.
#ifdef __APPLE__
constexpr size_t FEX_HOST_PAGE_SIZE = 16384;
constexpr size_t FEX_HOST_PAGE_SHIFT = 14;
#else
constexpr size_t FEX_HOST_PAGE_SIZE = FEX_PAGE_SIZE;
constexpr size_t FEX_HOST_PAGE_SHIFT = FEX_PAGE_SHIFT;
#endif
constexpr size_t FEX_HOST_PAGE_MASK = ~(FEX_HOST_PAGE_SIZE - 1);
} // namespace FEXCore::Utils
