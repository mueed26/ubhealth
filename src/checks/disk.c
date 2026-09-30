#include "ubhealth.h"

#include <mntent.h>
#include <string.h>
#include <sys/statvfs.h>

#define MAX_FS 64

/* Only real on-disk filesystems; skips tmpfs, squashfs (snaps), 9p, etc. */
static int is_local_fs(const char *type)
{
    static const char *const types[] = {
        "ext2", "ext3", "ext4", "xfs", "btrfs", "zfs", "f2fs", "vfat", NULL,
    };

    for (size_t i = 0; types[i]; i++)
        if (strcmp(type, types[i]) == 0)
            return 1;
    return 0;
}

/* Same formula as df: reserved blocks count as neither used nor available. */
double disk_used_pct(unsigned long long blocks, unsigned long long bfree,
                     unsigned long long bavail)
{
    unsigned long long used, denom;

    if (blocks < bfree)
        return 0.0;
    used = blocks - bfree;
    denom = used + bavail;
    if (denom == 0)
        return 0.0;
    return 100.0 * (double)used / (double)denom;
}

void check_disk(const config_t *cfg, check_result_t *out)
{
    char seen[MAX_FS][128];
    size_t nseen = 0;
    double worst_pct = -1.0;
    char worst_dir[256] = "";
    struct mntent *e;
    FILE *m = setmntent("/proc/mounts", "r");

    if (!m) {
        result_set(out, STATUS_SKIP, "Cannot read /proc/mounts");
        return;
    }
    while ((e = getmntent(m)) && nseen < MAX_FS) {
        struct statvfs st;
        double pct;
        int dup = 0;

        if (!is_local_fs(e->mnt_type))
            continue;
        /* the same device can be mounted at several points (bind mounts) */
        for (size_t i = 0; i < nseen; i++)
            if (strcmp(seen[i], e->mnt_fsname) == 0)
                dup = 1;
        if (dup || statvfs(e->mnt_dir, &st) != 0)
            continue;
        snprintf(seen[nseen++], sizeof seen[0], "%s", e->mnt_fsname);

        pct = disk_used_pct(st.f_blocks, st.f_bfree, st.f_bavail);
        result_detail(out, "%-24s %5.1f%% used  (%s)", e->mnt_dir, pct, e->mnt_fsname);
        if (pct > worst_pct) {
            worst_pct = pct;
            snprintf(worst_dir, sizeof worst_dir, "%s", e->mnt_dir);
        }
    }
    endmntent(m);

    if (nseen == 0) {
        result_set(out, STATUS_SKIP, "No local filesystems found");
        return;
    }
    status_t s = classify(worst_pct, cfg->disk_warn_pct, cfg->disk_crit_pct);
    if (s == STATUS_OK)
        result_set(out, s, "%zu filesystem(s) healthy, fullest is %s at %.0f%%",
                   nseen, worst_dir, worst_pct);
    else
        result_set(out, s, "%s is %.0f%% full", worst_dir, worst_pct);
}
