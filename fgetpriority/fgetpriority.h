/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * fgetpriority — KPM port
 *
 * Ported from the fgetpriority LKM (hooks sys_getpriority / sys_setpriority /
 * sys_ioprio_get / sys_ioprio_set indirectly by hooking their shared helper
 * find_user()) to a KernelPatch Kernel Patch Module (KPM).
 *
 * Original LKM:
 *   Copyright (C) 2026 \xx
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __FGETPRIORITY_KPM_H
#define __FGETPRIORITY_KPM_H

#include <hook.h>
#include <ksyms.h>
#include <kputils.h>
#include <kpmodule.h>
#include <uapi/asm-generic/errno.h>

/* --- uid ranges, mirrored from the LKM's routine.h ------------------- */

/* Single source of truth for the human-readable module version: used both
 * for KPM_VERSION() (the loader-visible metadata) and for the `status`
 * control0 reply, so the two can never drift apart. This is independent of
 * the Makefile's FGETPRIORITY_VERSION, which only names the output file. */
#define FGP_KPM_VERSION "0.0.4-kpm2"

#define MIN_APP_PROCESS_UID      10000
#define MIN_ISOLATED_PROCESS_UID 90000
#define AID_USER_OFFSET          100000

/* (uid_t)-1 is unmappable; it cannot own tasks, so find_user() will miss and
 * the caller sees the uid as if it did not exist — same trick the LKM's
 * kprobe fallback (routine_kp.c) used. */
#define HIDDEN_PRIORITY_UID ((uid_t)-1)

#define fgp_log(fmt, ...) pr_info("fgetpriority: " fmt, ##__VA_ARGS__)
#define fgp_warn(fmt, ...) pr_warn("fgetpriority: " fmt, ##__VA_ARGS__)
#define fgp_err(fmt, ...) pr_err("fgetpriority: " fmt, ##__VA_ARGS__)

#endif /* __FGETPRIORITY_KPM_H */
