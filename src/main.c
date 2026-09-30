#include "ubhealth.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { EXIT_WARN = 1, EXIT_CRIT = 2, EXIT_USAGE = 3 };

static const config_t DEFAULT_CONFIG = {
    .disk_warn_pct = 80.0,     .disk_crit_pct = 90.0,
    .mem_warn_pct = 85.0,      .mem_crit_pct = 95.0,
    .load_warn_per_cpu = 1.0,  .load_crit_per_cpu = 2.0,
};

static void usage(FILE *f)
{
    fprintf(f,
        "Usage: ubhealth [OPTIONS]\n"
        "Quick health report for an Ubuntu system.\n\n"
        "  -j, --json          output machine-readable JSON\n"
        "  -o, --only LIST     run only these checks (comma separated)\n"
        "  -l, --list          list available checks and exit\n"
        "  -v, --verbose       show details for every check, not just problems\n"
        "  -n, --no-color      disable coloured output (also honours NO_COLOR)\n"
        "  -h, --help          show this help and exit\n"
        "  -V, --version       show version and exit\n\n"
        "Exit status: 0 healthy, 1 warning, 2 critical, 3 usage error.\n");
}

static int find_check(const char *name)
{
    for (size_t i = 0; i < CHECK_COUNT; i++)
        if (strcmp(CHECKS[i].name, name) == 0)
            return (int)i;
    return -1;
}

int main(int argc, char **argv)
{
    static const struct option opts[] = {
        { "json",     no_argument,       NULL, 'j' },
        { "only",     required_argument, NULL, 'o' },
        { "list",     no_argument,       NULL, 'l' },
        { "verbose",  no_argument,       NULL, 'v' },
        { "no-color", no_argument,       NULL, 'n' },
        { "help",     no_argument,       NULL, 'h' },
        { "version",  no_argument,       NULL, 'V' },
        { NULL, 0, NULL, 0 },
    };
    int json = 0, verbose = 0, c;
    int color = isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
    char *only = NULL;

    while ((c = getopt_long(argc, argv, "jo:lvnhV", opts, NULL)) != -1) {
        switch (c) {
        case 'j': json = 1; break;
        case 'o': only = optarg; break;
        case 'v': verbose = 1; break;
        case 'n': color = 0; break;
        case 'l':
            for (size_t i = 0; i < CHECK_COUNT; i++)
                printf("%-10s %s\n", CHECKS[i].name, CHECKS[i].description);
            return 0;
        case 'h': usage(stdout); return 0;
        case 'V': printf("ubhealth %s\n", UBHEALTH_VERSION); return 0;
        default:  usage(stderr); return EXIT_USAGE;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "ubhealth: unexpected argument '%s'\n", argv[optind]);
        usage(stderr);
        return EXIT_USAGE;
    }

    int *selected = calloc(CHECK_COUNT, sizeof *selected);
    run_entry_t *entries = calloc(CHECK_COUNT, sizeof *entries);
    if (!selected || !entries) {
        perror("ubhealth");
        return EXIT_FAILURE;
    }
    if (only) {
        char *save = NULL;

        for (char *tok = strtok_r(only, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
            int idx = find_check(tok);

            if (idx < 0) {
                fprintf(stderr, "ubhealth: unknown check '%s' (see --list)\n", tok);
                free(selected);
                free(entries);
                return EXIT_USAGE;
            }
            selected[idx] = 1;
        }
    } else {
        for (size_t i = 0; i < CHECK_COUNT; i++)
            selected[i] = 1;
    }

    size_t n = 0;
    for (size_t i = 0; i < CHECK_COUNT; i++) {
        if (!selected[i])
            continue;
        entries[n].check = &CHECKS[i];
        CHECKS[i].run(&DEFAULT_CONFIG, &entries[n].result);
        n++;
    }

    if (json)
        report_json(stdout, entries, n);
    else
        report_text(stdout, entries, n, color, verbose);

    status_t overall = overall_status(entries, n);
    free(selected);
    free(entries);
    if (overall == STATUS_CRIT)
        return EXIT_CRIT;
    if (overall == STATUS_WARN)
        return EXIT_WARN;
    return 0;
}
