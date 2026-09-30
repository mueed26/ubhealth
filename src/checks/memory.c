#include "ubhealth.h"

/* Extract MemTotal and MemAvailable (both in kB) from /proc/meminfo. */
int parse_meminfo(FILE *f, unsigned long *total_kb, unsigned long *avail_kb)
{
    char line[256];
    int found = 0;

    *total_kb = *avail_kb = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned long v;

        if (sscanf(line, "MemTotal: %lu kB", &v) == 1) {
            *total_kb = v;
            found |= 1;
        } else if (sscanf(line, "MemAvailable: %lu kB", &v) == 1) {
            *avail_kb = v;
            found |= 2;
        }
    }
    return found == 3 ? 0 : -1;
}

void check_memory(const config_t *cfg, check_result_t *out)
{
    unsigned long total, avail;
    FILE *f = fopen("/proc/meminfo", "r");
    int rc;

    if (!f) {
        result_set(out, STATUS_SKIP, "Cannot read /proc/meminfo");
        return;
    }
    rc = parse_meminfo(f, &total, &avail);
    fclose(f);
    if (rc != 0 || total == 0) {
        result_set(out, STATUS_SKIP, "Unexpected /proc/meminfo format");
        return;
    }
    if (avail > total)
        avail = total;

    double pct = 100.0 * (double)(total - avail) / (double)total;
    result_set(out, classify(pct, cfg->mem_warn_pct, cfg->mem_crit_pct),
               "%.0f%% of RAM in use (%.1f / %.1f GiB)", pct,
               (double)(total - avail) / 1048576.0, (double)total / 1048576.0);
}