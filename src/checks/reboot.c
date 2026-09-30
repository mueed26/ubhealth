#include "ubhealth.h"

#include <string.h>
#include <unistd.h>

/* Created by update-notifier when an upgraded package (e.g. the kernel) needs a reboot. */
#define REBOOT_FLAG "/var/run/reboot-required"
#define REBOOT_PKGS "/var/run/reboot-required.pkgs"

void check_reboot(const config_t *cfg, check_result_t *out)
{
    char line[256];
    FILE *f;

    (void)cfg;
    if (access(REBOOT_FLAG, F_OK) != 0) {
        result_set(out, STATUS_OK, "No reboot required");
        return;
    }
    f = fopen(REBOOT_PKGS, "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            line[strcspn(line, "\n")] = '\0';
            if (*line)
                result_detail(out, "required by %s", line);
        }
        fclose(f);
    }
    result_set(out, STATUS_WARN, "A reboot is required to finish applying updates");
}
