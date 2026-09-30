#include "ubhealth.h"

/* Order here is the order checks appear in the report. */
const check_t CHECKS[] = {
    { "disk",     "Local filesystem usage",          check_disk },
    { "memory",   "RAM usage",                       check_memory },
    { "load",     "CPU load average per core",       check_load },
    { "updates",  "Pending apt and security updates", check_updates },
    { "services", "Failed systemd units",            check_services },
    { "reboot",   "Reboot required after updates",   check_reboot },
};

const size_t CHECK_COUNT = sizeof CHECKS / sizeof CHECKS[0];
