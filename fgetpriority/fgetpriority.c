/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * fgetpriority — KPM port
 *
 * Original LKM filtered sys_getpriority / sys_setpriority / sys_ioprio_get /
 * sys_ioprio_set by self-patching ARM64 B/BL call sites (routine_raw.c) to
 * redirect find_user() to a shadow gphook_find_user(), with a kprobe
 * fallback (routine_kp.c) that rewrote find_user()'s first argument before
 * the real function ran.
 *
 * KernelPatch already does inline function hooking for us (hook_wrap), so
 * this port keeps only the kprobe-style strategy: hook find_user() once and
 * rewrite its kuid_t argument in the "before" callback. Because hook_wrap
 * patches find_user() itself rather than its call sites, every caller
 * (sys_getpriority, sys_setpriority, sys_ioprio_get, sys_ioprio_set) is
 * covered automatically — the LKM's four separate call-site patches are not
 * needed here.
 *
 * KNOWN DEVIATIONS FROM THE LKM (see fgetpriority.h / README for detail):
 *   - The isolated/work-profile uid -> host uid translation via
 *     current_cred()->user_ns (make_kuid/from_kuid) is NOT ported: this KPM
 *     build environment only exposes forward-declared, offset-indirected
 *     struct cred / user_namespace, so touching those fields directly here
 *     would mean guessing per-kernel-version layouts, which is how kernel
 *     modules crash devices. Plain modulo-AID_USER_OFFSET normalization is
 *     kept, which covers the common (non-namespaced) isolated-uid case.
 *   - The is_seccomp_enabled() gate (TIF_SECCOMP) is NOT ported, for the
 *     same reason: thread_info flag layout isn't exposed here. Since the
 *     filter already requires both uids to be >= MIN_APP_PROCESS_UID, this
 *     only widens the filter to app-uid callers that aren't currently
 *     seccomp-filtered, which in practice is rare on Android.
 * Both can be added back with hook-based struct-offset resolution if you
 * need them; ask and I'll wire that up against a specific kernel/KMI.
 */

#include <linux/kernel.h>
#include <linux/printk.h>

#include "fgetpriority.h"

KPM_NAME("fgetpriority");
KPM_VERSION("0.0.4-kpm1");
KPM_LICENSE("GPL-2.0-only");
KPM_AUTHOR("xx, frknkrc44");
KPM_DESCRIPTION("Filters sys_getpriority/setpriority/ioprio_get/ioprio_set to hide cross-app uid relationships");

/*
 * find_user(kuid_t uid) has had this exact signature in kernel/user.c since
 * kuid_t was introduced (long before 4.19) and is still present, unchanged,
 * through the upper end of KernelPatch's supported range (Linux 3.18-6.6,
 * per KernelPatch's own README), so no per-version branching is needed here
 * — the only moving part is whether kallsyms can resolve the symbol on a
 * given build, which is already handled as a plain init-time failure below.
 */

static void *g_find_user_addr;
static bool g_hooked;

static __always_inline bool fgp_should_swap(uid_t who)
{
    uid_t cuid = current_uid();

    /* only filter appuid <-> appuid calls */
    if (cuid < MIN_APP_PROCESS_UID || who < MIN_APP_PROCESS_UID)
        return false;

    if (cuid >= AID_USER_OFFSET)
        cuid %= AID_USER_OFFSET;

    if (who >= AID_USER_OFFSET)
        who %= AID_USER_OFFSET;

    if (cuid == who)
        return false;

    return true;
}

/*
 * struct user_struct *find_user(kuid_t uid);
 *
 * kuid_t is a single-member struct { uid_t val; }, passed/returned in the
 * same register a plain uid_t would use, so arg0's low 32 bits are the uid.
 */
static void before_find_user(hook_fargs1_t *args, void *udata)
{
    uid_t who = (uid_t)(args->arg0 & 0xffffffffu);

    if (fgp_should_swap(who))
        args->arg0 = (uint64_t)(uint32_t)HIDDEN_PRIORITY_UID;
}

static long fgetpriority_init(const char *args, const char *event, void *__user reserved)
{
    hook_err_t err;

    fgp_log("init event=%s\n", event ?: "(null)");

    g_find_user_addr = (void *)kallsyms_lookup_name("find_user");
    if (!g_find_user_addr) {
        fgp_err("cannot find find_user\n");
        return -ENOENT;
    }
    fgp_log("find_user @ %px\n", g_find_user_addr);

    err = hook_wrap1(g_find_user_addr, before_find_user, NULL, NULL);
    if (err) {
        fgp_err("hook find_user failed: %d\n", err);
        return -EAGAIN;
    }
    g_hooked = true;

    fgp_log("init: ok!\n");
    return 0;
}

static long fgetpriority_control0(const char *args, char *__user out_msg, int outlen)
{
    /* no runtime controls exposed yet */
    return -EINVAL;
}

static long fgetpriority_exit(void *__user reserved)
{
    if (g_hooked && g_find_user_addr) {
        hook_unwrap(g_find_user_addr, before_find_user, NULL);
        g_hooked = false;
    }

    fgp_log("exit: Goodbye!\n");
    return 0;
}

KPM_INIT(fgetpriority_init);
KPM_CTL0(fgetpriority_control0);
KPM_EXIT(fgetpriority_exit);
