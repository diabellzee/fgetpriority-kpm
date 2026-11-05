# fgetpriority (KPM)

A [KernelPatch](https://github.com/bmax121/KernelPatch) KPM port of the
[fgetpriority](https://github.com/) LKM: filters `sys_getpriority` /
`sys_setpriority` / `sys_ioprio_get` / `sys_ioprio_set`.

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

## License

GPL-2.0-only — see [LICENSE](./LICENSE). Original LKM Copyright (C) 2026 xx;
KPM port keeps the same license and credits the original author(s)
(`xx`, `frknkrc44`).
