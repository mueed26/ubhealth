#include "ubhealth.h"

#include <string.h>
#include <unistd.h>

// Ships with update-notifier-common on Ubuntu and alsoo prints "UPDATES;SECURITY" to stderr.

#define APT_CHECK "/usr/lib/update-notifier/apt-check"

//Uses the last line that looks like "N;M", since apt may print warnings first.
int parse_apt_check(const char *output, int *updates, int *security)
{
    const char *p = output;
    int found = -1;

    while (*p) {
        int u, s;

        if (sscanf(p, "%d;%d", &u, &s) == 2) {
            *updates = u;
            *security = s;
            found = 0;
        }
        p = strchr(p, '\n');
        if (!p)
            break;
        p++;
    }
    return found;
}

int count_upgradable(const char *apt_list_output)
{
    int n = 0;

    for (const char *p = apt_list_output; (p = strstr(p, "[upgradable from")); p++)
        n++;
    return n;
}

void check_updates(const config_t *cfg, check_result_t *out)
{
    static char buf[256 * 1024];
    int code, updates = 0, security = -1;

    (void)cfg;
    if (access(APT_CHECK, X_OK) == 0) {
        if (run_command(APT_CHECK " 2>&1", buf, sizeof buf, &code) != 0 ||
            parse_apt_check(buf, &updates, &security) != 0) {
            result_set(out, STATUS_SKIP, "apt-check gave unexpected output");
            return;
        }
    } else if (access("/usr/bin/apt", X_OK) == 0) {
        // fallback: no security breakdown available 
        if (run_command("apt list --upgradable 2>/dev/null", buf, sizeof buf, &code) != 0) {
            result_set(out, STATUS_SKIP, "Could not run apt");
            return;
        }
        updates = count_upgradable(buf);
    } else {
        result_set(out, STATUS_SKIP, "apt not found (not a Debian/Ubuntu system?)");
        return;
    }

    if (security > 0)
        result_set(out, STATUS_CRIT, "%d update(s) pending, %d of them security", updates, security);
    else if (updates > 0)
        result_set(out, STATUS_WARN, "%d update(s) pending", updates);
    else
        result_set(out, STATUS_OK, "System is up to date");
    if (updates > 0)
        result_detail(out, "Run: sudo apt update && sudo apt upgrade");
}
