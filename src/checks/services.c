#include "ubhealth.h"

#include <string.h>

/*
 * Parse `systemctl --failed --no-legend --plain` output: one unit per line,
 * unit name first. Adds each unit name to out->detail (if out is non-NULL).
 */
int parse_failed_units(const char *output, check_result_t *out)
{
    const char *p = output;
    int n = 0;

    while (*p) {
        const char *end = strchr(p, '\n');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        size_t i = 0, j;

        while (i < len && (p[i] == ' ' || p[i] == '\t'))
            i++;
        if (i < len) {
            j = i;
            while (j < len && p[j] != ' ' && p[j] != '\t')
                j++;
            if (out)
                result_detail(out, "%.*s", (int)(j - i), p + i);
            n++;
        }
        if (!end)
            break;
        p = end + 1;
    }
    return n;
}

void check_services(const config_t *cfg, check_result_t *out)
{
    char buf[16384];
    int code, n;

    (void)cfg;
    if (run_command("systemctl --failed --no-legend --plain 2>/dev/null",
                    buf, sizeof buf, &code) != 0 || code != 0) {
        result_set(out, STATUS_SKIP, "systemd is not running (container, or WSL without systemd?)");
        return;
    }
    n = parse_failed_units(buf, out);
    if (n == 0)
        result_set(out, STATUS_OK, "No failed systemd units");
    else
        result_set(out, STATUS_WARN, "%d failed systemd unit%s", n, n == 1 ? "" : "s");
}
