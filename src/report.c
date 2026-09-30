#include "ubhealth.h"

#include <string.h>
#include <unistd.h>

#define C_RESET "\033[0m"
#define C_BOLD  "\033[1m"
#define C_DIM   "\033[2m"
#define C_RED   "\033[31m"
#define C_GREEN "\033[32m"
#define C_YEL   "\033[33m"

/* width of "  [WARN] " plus a 9-char name column and a space */
#define DETAIL_INDENT 19

status_t overall_status(const run_entry_t *entries, size_t n)
{
    status_t worst = STATUS_SKIP;

    for (size_t i = 0; i < n; i++)
        if (status_severity(entries[i].result.status) > status_severity(worst))
            worst = entries[i].result.status;
    return worst;
}

static void hostname(char *buf, size_t size)
{
    if (gethostname(buf, size) != 0)
        snprintf(buf, size, "unknown");
    buf[size - 1] = '\0';
}

static const char *label(status_t s)
{
    switch (s) {
    case STATUS_OK:   return " OK ";
    case STATUS_WARN: return "WARN";
    case STATUS_CRIT: return "CRIT";
    case STATUS_SKIP: return "SKIP";
    }
    return "????";
}

static const char *colour(status_t s)
{
    switch (s) {
    case STATUS_OK:   return C_GREEN;
    case STATUS_WARN: return C_YEL;
    case STATUS_CRIT: return C_RED;
    case STATUS_SKIP: return C_DIM;
    }
    return "";
}

void report_text(FILE *f, const run_entry_t *entries, size_t n, int color, int verbose)
{
    const char *bold = color ? C_BOLD : "", *reset = color ? C_RESET : "";
    int counts[4] = { 0 };
    char host[256];

    hostname(host, sizeof host);
    fprintf(f, "%subhealth %s%s - system health report for %s\n\n",
            bold, UBHEALTH_VERSION, reset, host);

    for (size_t i = 0; i < n; i++) {
        const check_result_t *r = &entries[i].result;
        int noisy = r->status == STATUS_WARN || r->status == STATUS_CRIT;

        counts[r->status]++;
        fprintf(f, "  %s[%s]%s %-9s %s\n", color ? colour(r->status) : "",
                label(r->status), reset, entries[i].check->name, r->summary);

        if (!(verbose || noisy) || !*r->detail)
            continue;
        for (const char *p = r->detail; *p;) {
            const char *end = strchr(p, '\n');
            int len = end ? (int)(end - p) : (int)strlen(p);

            fprintf(f, "%*s%s%.*s%s\n", DETAIL_INDENT, "", color ? C_DIM : "", len, p, reset);
            if (!end)
                break;
            p = end + 1;
        }
    }
    fprintf(f, "\n  %d ok, %d warning, %d critical, %d skipped\n",
            counts[STATUS_OK], counts[STATUS_WARN], counts[STATUS_CRIT], counts[STATUS_SKIP]);
}

void report_json(FILE *f, const run_entry_t *entries, size_t n)
{
    char host[256];

    hostname(host, sizeof host);
    fprintf(f, "{\n  \"version\": ");
    json_write_string(f, UBHEALTH_VERSION);
    fprintf(f, ",\n  \"hostname\": ");
    json_write_string(f, host);
    fprintf(f, ",\n  \"overall\": \"%s\",\n  \"checks\": [", status_name(overall_status(entries, n)));

    for (size_t i = 0; i < n; i++) {
        const check_result_t *r = &entries[i].result;
        int first = 1;

        fprintf(f, "%s\n    {\"name\": ", i ? "," : "");
        json_write_string(f, entries[i].check->name);
        fprintf(f, ", \"status\": \"%s\", \"summary\": ", status_name(r->status));
        json_write_string(f, r->summary);
        fprintf(f, ", \"details\": [");
        for (const char *p = r->detail; *p;) {
            const char *end = strchr(p, '\n');
            size_t len = end ? (size_t)(end - p) : strlen(p);

            fprintf(f, "%s", first ? "" : ", ");
            json_write_stringn(f, p, len);
            first = 0;
            if (!end)
                break;
            p = end + 1;
        }
        fprintf(f, "]}");
    }
    fprintf(f, "\n  ]\n}\n");
}
