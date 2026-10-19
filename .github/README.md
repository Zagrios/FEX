# FEX for macOS

**[FEX-Emu](https://github.com/FEX-Emu/FEX) with a Darwin port, for Voile's Linux engine.** FEXCore translates x86-64 code to arm64; this fork lets it run inside a macOS arm64 process. [voile-lkd](https://github.com/Zagrios/voile-lkd)'s host program, `fexdarwin-run`, embeds it and serves the guest's Linux system calls itself.

This page is the fork's own. FEX's README is [Readme.md](../Readme.md), unchanged.

## What the port changes

The port is a few commits on top of FEX; `git log upstream..main` lists them. The first, `FEXCore: Run on macOS arm64 hosts.`, carries most of it:

- Guest memory sits in a window at a host offset (`GuestBase.h`, x24 in the JIT), so a 47-bit guest address space fits beside the macOS process.
- JIT code pages follow Apple's W^X rules (`JITWriteScope.h`, `MAP_JIT` and `pthread_jit_write_protect_np`).
- The host can ask whether a host PC sits exactly on a guest instruction boundary, so asynchronous signals interrupt translated code only where the guest state is complete.
- Self-modifying code is tracked per host page through the host's callbacks.
- Segment bases account for the guest window; RDTSCP and RDPID work on Apple silicon.
- Linux-only allocator, prctl, file and profiler helpers get Darwin equivalents or are compiled out.

The next ones fix what games found since: code checks per instruction on pages the frontend names, no checksum for instructions without bytes, and a thread creation that fails cleanly when its lookup cache cannot be allocated.

Only FEXCore is ported: on macOS, CMake builds FEXCore alone, without FEX's tools under `Source/`, its thunks or its Linux data files.

## Branches

- `upstream` holds FEX-Emu's `main` as last imported.
- `main` is `upstream` plus the Darwin port, rebased on `upstream` after an import:

```sh
git remote add upstream-fex https://github.com/FEX-Emu/FEX.git   # once
git fetch upstream-fex
git branch -f upstream upstream-fex/main
git rebase upstream main
```

## Building

voile-lkd uses this repository as its `fex` submodule and builds FEXCore with `fexdarwin-run`: see [its README](https://github.com/Zagrios/voile-lkd#building).

## License

FEX is MIT licensed: see [LICENSE](../LICENSE).

## Related repositories

| Repository | Role |
|---|---|
| [Obelisk](https://github.com/Zagrios/obelisk) | The Mac app: Steam, Epic Games and GOG libraries, downloads and launches |
| [Voile](https://github.com/Zagrios/voile) | The Windows runtime Obelisk installs |
| [voile-wine](https://github.com/Zagrios/voile-wine) | The Wine inside Voile, for both engines |
| [voile-lkd](https://github.com/Zagrios/voile-lkd) | The Linux engine, which embeds this FEXCore |
| [Neptune](https://github.com/Zagrios/neptune) | Direct3D for the Linux engine |
| **FEX** | FEX-Emu with its macOS port |
