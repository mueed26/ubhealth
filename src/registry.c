#include "ubhealth.h"

/* Order here is the order checks appear in the report. */
const check_t CHECKS[] = {
    { "disk",     "Local filesystem usage",          check_disk },
};

const size_t CHECK_COUNT = sizeof CHECKS / sizeof CHECKS[0];
