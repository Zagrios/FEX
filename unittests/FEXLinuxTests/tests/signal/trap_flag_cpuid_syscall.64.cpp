#include <catch2/catch_test_macros.hpp>

#include <iterator>
#include <signal.h>
#include <stdint.h>
#include <ucontext.h>

// Single-stepping with TF over CPUID and SYSCALL.
//
// Both instructions leave the JIT block: CPUID runs through a dispatcher stub, SYSCALL through the syscall handler,
// and both return to guest code through the L1 cache lookup. With TF set the trap must still land on the instruction
// after CPUID/SYSCALL and after every instruction that follows. Each probe is run once with TF clear first, so the
// code after the instruction is compiled and in the cache when the stepped run reaches it.

#pragma GCC diagnostic ignored "-Wattributes" // Suppress warning in case control-flow checks aren't enabled

extern "C" void ProbeCPUID();
extern "C" void ProbeCPUIDWarm();
extern "C" void TF_CPUID_1();
extern "C" void TF_CPUID_2();
extern "C" void TF_CPUID_3();
extern "C" void TF_CPUID_4();
extern "C" void TF_CPUID_5();
extern "C" void TF_CPUID_6();
extern "C" void TF_CPUID_7();
extern "C" void TF_CPUID_8();
extern "C" void TF_CPUID_9();

// Sets TF, then every instruction up to and including the POPFQ that clears it traps once.
extern "C" __attribute__((naked, nocf_check)) void ProbeCPUID() {
  __asm volatile(R"(
  pushfq;
  or qword ptr [rsp], 0x100;
  popfq;
  ProbeCPUIDWarm:
  push rbx;
  TF_CPUID_1:
  xor eax, eax;
  TF_CPUID_2:
  cpuid;
  TF_CPUID_3:
  nop;
  TF_CPUID_4:
  nop;
  TF_CPUID_5:
  pop rbx;
  TF_CPUID_6:
  pushfq;
  TF_CPUID_7:
  and qword ptr [rsp], ~0x100;
  TF_CPUID_8:
  popfq;
  TF_CPUID_9:
  ret;
  )");
}

extern "C" void ProbeSYSCALL();
extern "C" void ProbeSYSCALLWarm();
extern "C" void TF_SYSCALL_1();
extern "C" void TF_SYSCALL_2();
extern "C" void TF_SYSCALL_3();
extern "C" void TF_SYSCALL_4();
extern "C" void TF_SYSCALL_5();
extern "C" void TF_SYSCALL_6();
extern "C" void TF_SYSCALL_7();

extern "C" __attribute__((naked, nocf_check)) void ProbeSYSCALL() {
  __asm volatile(R"(
  pushfq;
  or qword ptr [rsp], 0x100;
  popfq;
  ProbeSYSCALLWarm:
  mov eax, 39;
  TF_SYSCALL_1:
  syscall;
  TF_SYSCALL_2:
  nop;
  TF_SYSCALL_3:
  nop;
  TF_SYSCALL_4:
  pushfq;
  TF_SYSCALL_5:
  and qword ptr [rsp], ~0x100;
  TF_SYSCALL_6:
  popfq;
  TF_SYSCALL_7:
  ret;
  )");
}

constexpr int MAX_TRAPS = 32;
static uintptr_t trap_rips[MAX_TRAPS];
static int trap_count;
static int trap_bad_si_code;

static void TrapHandler(int, siginfo_t* siginfo, void* context) {
  auto* ctx = static_cast<ucontext_t*>(context);
  if (trap_count < MAX_TRAPS) {
    trap_rips[trap_count] = ctx->uc_mcontext.gregs[REG_RIP];
  }
  trap_count++;
  if (siginfo->si_code != TRAP_TRACE) {
    trap_bad_si_code++;
  }
}

static void InstallTrapHandler() {
  struct sigaction act {};
  act.sa_sigaction = TrapHandler;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGTRAP, &act, nullptr);
  trap_count = 0;
  trap_bad_si_code = 0;
}

template<size_t N>
static void CheckTraps(const uintptr_t (&expected)[N]) {
  REQUIRE(trap_count == N);
  CHECK(trap_bad_si_code == 0);
  for (size_t i = 0; i < N; ++i) {
    INFO("trap " << i);
    CHECK(trap_rips[i] == expected[i]);
  }
}

TEST_CASE("Signals: Trap Flag across CPUID") {
  ProbeCPUIDWarm();
  InstallTrapHandler();
  ProbeCPUID();

  const uintptr_t expected[] = {
    reinterpret_cast<uintptr_t>(&TF_CPUID_1), reinterpret_cast<uintptr_t>(&TF_CPUID_2), reinterpret_cast<uintptr_t>(&TF_CPUID_3),
    reinterpret_cast<uintptr_t>(&TF_CPUID_4), reinterpret_cast<uintptr_t>(&TF_CPUID_5), reinterpret_cast<uintptr_t>(&TF_CPUID_6),
    reinterpret_cast<uintptr_t>(&TF_CPUID_7), reinterpret_cast<uintptr_t>(&TF_CPUID_8), reinterpret_cast<uintptr_t>(&TF_CPUID_9),
  };
  CheckTraps(expected);
}

TEST_CASE("Signals: Trap Flag across SYSCALL") {
  ProbeSYSCALLWarm();
  InstallTrapHandler();
  ProbeSYSCALL();

  // Where the trap after SYSCALL lands depends on the kernel's return path. FEX and a FRED kernel trap before the
  // next instruction runs. A kernel returning through IRET does not trap after the IRET itself, so the first trap
  // after the syscall lands after the following instruction.
  const uintptr_t trap_after_syscall[] = {
    reinterpret_cast<uintptr_t>(&TF_SYSCALL_1), reinterpret_cast<uintptr_t>(&TF_SYSCALL_2), reinterpret_cast<uintptr_t>(&TF_SYSCALL_3),
    reinterpret_cast<uintptr_t>(&TF_SYSCALL_4), reinterpret_cast<uintptr_t>(&TF_SYSCALL_5), reinterpret_cast<uintptr_t>(&TF_SYSCALL_6),
    reinterpret_cast<uintptr_t>(&TF_SYSCALL_7),
  };
  const uintptr_t trap_after_next[] = {
    reinterpret_cast<uintptr_t>(&TF_SYSCALL_1), reinterpret_cast<uintptr_t>(&TF_SYSCALL_3), reinterpret_cast<uintptr_t>(&TF_SYSCALL_4),
    reinterpret_cast<uintptr_t>(&TF_SYSCALL_5), reinterpret_cast<uintptr_t>(&TF_SYSCALL_6), reinterpret_cast<uintptr_t>(&TF_SYSCALL_7),
  };
  if (trap_count == std::size(trap_after_next)) {
    CheckTraps(trap_after_next);
  } else {
    CheckTraps(trap_after_syscall);
  }
}
