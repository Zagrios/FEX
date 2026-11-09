# FEX for macOS

**[FEX-Emu](https://github.com/FEX-Emu/FEX) with a Darwin port, for Voile's Linux engine.** FEXCore translates x86-64 code to arm64; this fork lets it run inside a macOS arm64 process. [voile-lkd](https://github.com/Zagrios/voile-lkd)'s host program, `fexdarwin-run`, embeds it and serves the guest's Linux system calls itself.

This page is the fork's own. FEX's README is [Readme.md](../Readme.md), unchanged.

## What the port changes

The port is a series of commits merged with FEX's `main`; `git log --no-merges upstream-fex/main..main` lists them (see [Branches](#branches)). The first, `FEXCore: Run on macOS arm64 hosts.`, carries most of it:

- Guest memory sits in a window at a host offset (`GuestBase.h`, x24 in the JIT), so a 47-bit guest address space fits beside the macOS process.
- JIT code pages follow Apple's W^X rules (`JITWriteScope.h`, `MAP_JIT` and `pthread_jit_write_protect_np`).
- The host can ask whether a host PC sits exactly on a guest instruction boundary, so asynchronous signals interrupt translated code only where the guest state is complete.
- Self-modifying code is tracked per host page through the host's callbacks.
- Segment bases account for the guest window; RDTSCP and RDPID work on Apple silicon.
- Linux-only allocator, prctl, file and profiler helpers get Darwin equivalents or are compiled out.

The later ones fix what games found since, among them: code checks per instruction on pages the frontend names, code read from guarded pages through views the host gives, 32-bit code in a 64-bit process up to the 4 GiB wrap of EIP, the single-step trap and #UD and #GP where x86 raises them, and ring 3 segment checks answered as Linux on x86 does. The ones after a merge with FEX-Emu's `main` keep those rules on the paths the merge adds.

Only FEXCore is ported: on macOS, CMake builds FEXCore alone, without FEX's tools under `Source/`, its thunks or its Linux data files.

## Branches

`main` is the Darwin port merged with FEX-Emu's `main`. An import merges FEX-Emu's `main` into it and never rebases, so every commit that voile-lkd has pinned as its `fex` submodule stays reachable from `main`:

```sh
git remote add upstream-fex https://github.com/FEX-Emu/FEX.git   # once
git fetch upstream-fex
git switch main
git merge --no-ff upstream-fex/main -m "Merge FEX-Emu's main up to $(git rev-parse --short upstream-fex/main)"
```

The merge message lists each conflict and how it was resolved. What the new upstream code needs from the port, such as a fork fix that a new upstream path bypasses, goes in separate commits after the merge. A pull request that carries a merge lands by fast-forwarding `main` to its head, or with GitHub's merge commit, never by squash or rebase, which would drop the merge.

FEXCore reaches Voile only through voile-lkd: once `main` has moved, a voile-lkd commit moves its `fex` submodule to the new head, together with any change `fexdarwin-run` needs to build against it. The next Voile release builds that commit.

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
