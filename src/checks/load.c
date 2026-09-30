#include "ubhealth.h"

#include <unistd.h>

int parse_loadavg(const char *s, double load[3])
{
    return sscanf(s, "%lf %lf %lf", &load[0], &load[1], &load[2]) == 3 ? 0 : -1;
}

void check_load(const config_t *cfg, check_result_t *out)
{
    char buf[128];
    double load[3];
    long cpus;

    if (read_first_line("/proc/loadavg", buf, sizeof buf) != 0 ||
        parse_loadavg(buf, load) != 0) {
        result_set(out, STATUS_SKIP, "Cannot read /proc/loadavg");
        return;
    }
    cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1)
        cpus = 1;

    /* the 5-minute average ignores short spikes but still reacts quickly */
    double per_cpu = load[1] / (double)cpus;
    result_set(out, classify(per_cpu, cfg->load_warn_per_cpu, cfg->load_crit_per_cpu),
               "Load %.2f %.2f %.2f across %ld CPU(s)", load[0], load[1], load[2], cpus);
    result_detail(out, "5-minute load per CPU: %.2f (warn at %.2f, critical at %.2f)",
                  per_cpu, cfg->load_warn_per_cpu, cfg->load_crit_per_cpu);
}