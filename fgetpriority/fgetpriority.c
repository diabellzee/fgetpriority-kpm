/* SPDX-License-Identifier: GPL-2.0-only */

#include <linux/kernel.h>
#include <linux/printk.h>
#include "fgetpriority.h"

KPM_NAME("fgetpriority");
KPM_VERSION(FGP_KPM_VERSION);
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
    uid_t who;

    (void)udata;
    who = (uid_t)(args->arg0 & 0xffffffffu);

    if (fgp_should_swap(who))
        args->arg0 = (uint64_t)(uint32_t)HIDDEN_PRIORITY_UID;
}

static long fgetpriority_init(const char *args, const char *event, void *__user reserved)
{
    hook_err_t err;

    (void)args;
    (void)reserved;

    if (g_hooked) {
        /* init() shouldn't normally run twice, but don't double-hook if it
         * ever does (e.g. a reload without a matching exit()). */
        fgp_log("init: already hooked, skipping\n");
        return 0;
    }

    fgp_log("init event=%s\n", event ?: "(null)");

    g_find_user_addr = (void *)kallsyms_lookup_name("find_user");
    if (!g_find_user_addr) {
        fgp_err("cannot find find_user\n");
        return -ENOENT;
    }
    /* %p (hashed), not %px: a module whose entire purpose is hiding things
     * from userspace shouldn't be the one leaking a real, KASLR-defeating
     * kernel address into dmesg, which unprivileged processes can often
     * still read depending on kptr_restrict/dmesg_restrict. */
    fgp_log("find_user resolved @ %p\n", g_find_user_addr);

    err = hook_wrap1(g_find_user_addr, before_find_user, NULL, NULL);
    if (err) {
        fgp_err("hook find_user failed: %d\n", err);
        return -EAGAIN;
    }
    g_hooked = true;

    fgp_log("init: ok!\n");
    return 0;
}

/* Minimal, dependency-free string compare -- avoids pulling in the
 * kfunc-resolved <linux/string.h> helpers (strcmp et al.) just for a
 * handful of fixed control-command literals. `args` comes from the
 * kpatch CLI / caller, not from an untrusted remote source, but we still
 * treat it as not-necessarily-NUL-bounded-sanely and cap the scan. */
static bool fgp_args_is(const char *args, const char *lit)
{
    int i;

    if (!args)
        return false;

    for (i = 0; i < 64; i++) {
        if (args[i] != lit[i])
            return false;
        if (args[i] == '\0')
            return true;
    }
    return false;
}

/*
 * `kpatch kpm control fgetpriority status` (or the APatch UI's module
 * control box) -> quick on-device sanity check without needing to grep
 * dmesg: is the hook actually installed right now, and does find_user
 * still resolve to the same address we hooked (catches the unlikely case
 * of something else having unhooked/rehooked it from under us).
 */
static long fgetpriority_control0(const char *args, char *__user out_msg, int outlen)
{
    char buf[96];
    int len;
    int copied;
    void *live_addr;

    if (!out_msg || outlen <= 0)
        return -EINVAL;

    if (!fgp_args_is(args, "status"))
        return -EINVAL;

    live_addr = g_hooked ? (void *)kallsyms_lookup_name("find_user") : NULL;

    len = snprintf(buf, sizeof(buf), "version=%s hooked=%d find_user_live=%d\n",
                   FGP_KPM_VERSION, g_hooked ? 1 : 0,
                   (g_hooked && live_addr == g_find_user_addr) ? 1 : 0);
    if (len <= 0)
        return -EFAULT;
    if (len >= (int)sizeof(buf))
        len = (int)sizeof(buf) - 1; /* truncated, still useful */
    if (len > outlen)
        len = outlen;

    copied = compat_copy_to_user(out_msg, buf, len);
    if (copied != len)
        return -EFAULT;

    return 0;
}

static long fgetpriority_exit(void *__user reserved)
{
    (void)reserved;

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
