# fgetpriority (KPM)

A [KernelPatch](https://github.com/bmax121/KernelPatch) KPM port of the
[fgetpriority](https://github.com/) LKM: filters `sys_getpriority` /
`sys_setpriority` / `sys_ioprio_get` / `sys_ioprio_set` so an app process
can't use them to detect another app's uid.

This is a KPM (`.kpm`), loadable by **APatch** or any KernelPatch-based
("FolkPatch") root solution — it is **not** an `insmod`-able `.ko` and does
not need `CONFIG_FGETPRIORITY` or any kernel rebuild.

## How it works

The original LKM patched the ARM64 call sites of `find_user()` directly
(with a kprobe fallback). KernelPatch already provides proper inline
function hooking (`hook_wrap`), so this port hooks `find_user(kuid_t uid)`
once, at the function itself, and rewrites its argument in the "before"
callback when the calling app's uid and the queried uid are different app
uids. Because the hook sits on `find_user()` rather than on its callers'
call sites, all four syscalls are covered by a single hook — the LKM's four
separate call-site patches aren't needed.

## Compatibility

- **arm64 only**, same as upstream KernelPatch.
- Tested to build cleanly against KernelPatch's stub headers (clang,
  `--target=aarch64-linux-gnu`, `-std=gnu99`) and by inspection against the
  CI toolchain below.
- `find_user(kuid_t uid)` has had this exact signature in `kernel/user.c`
  since `kuid_t` was introduced, long before 4.19, and is unchanged through
  Linux 6.6 — the top of KernelPatch's documented supported range (3.18–6.6)
  — so **no version-specific branching was needed** for the hook itself.
- Requires `CONFIG_KALLSYMS=y` on the target kernel (same requirement as
  KernelPatch itself), so `find_user` can be resolved by name.
- **Minimum verified target: Linux 4.19.191+.** Below that, only through
  KernelPatch's own stated floor (3.18) is it "theoretically" supported;
  above 6.6, it should keep working as long as `find_user()`'s signature
  doesn't change, which it hasn't on mainline since.

### Known deviations from the original LKM

These were intentionally **not** ported, because this KPM build environment
only exposes forward-declared `struct cred` / `struct user_namespace` behind
runtime-resolved offsets — guessing field layout here is exactly how a
kernel module panics a device, so neither was guessed at:

1. **Namespaced isolated/work-profile uid → host uid translation**
   (`current_cred()->user_ns`, `make_kuid`/`from_kuid` in the LKM). Plain
   `% AID_USER_OFFSET` normalization is kept, which covers the common,
   non-namespaced isolated-uid case.
2. **The `is_seccomp_enabled()` / `TIF_SECCOMP` gate.** Since the filter
   already requires both uids to be `>= MIN_APP_PROCESS_UID`, dropping this
   only widens the filter to app-uid callers that aren't currently
   seccomp-filtered — rare in practice on Android.

Both can be added back with offset-resolution against a specific
kernel/KMI if you need them — open an issue naming the target.

## Build

### Locally

```sh
git clone --recurse-submodules <this-repo>
cd fgetpriority-kpm
make -C fgetpriority TARGET_COMPILE=/path/to/aarch64-none-elf-
```

or, with the Android NDK on `PATH`/`ANDROID_NDK_HOME`:

```sh
make -C fgetpriority
```

The output is `fgetpriority/fgetpriority_<version>.kpm`.

### GitHub Actions

Push to `fgetpriority/**` or trigger the `Build KPM` workflow manually
(`workflow_dispatch`, optionally with `release: true` to also publish a
GitHub release). The workflow downloads the ARM GNU Toolchain
(`aarch64-none-elf-gcc` 12.2) and runs the same `make -C fgetpriority`.

## Install

Via APatch: Module manager → load `fgetpriority_<version>.kpm` directly, or
via `kpatch`:

```sh
kpatch kpm load /path/to/fgetpriority_<version>.kpm
```

## License

GPL-2.0-only — see [LICENSE](./LICENSE). Original LKM Copyright (C) 2026 xx;
KPM port keeps the same license and credits the original author(s)
(`xx`, `frknkrc44`).
