#ifndef UBHEALTH_H
#define UBHEALTH_H

#include <stddef.h>
#include <stdio.h>

#define UBHEALTH_VERSION "0.1.0"

#define SUMMARY_MAX 256
#define DETAIL_MAX 4096

typedef enum {
    STATUS_OK = 0,
    STATUS_WARN,
    STATUS_CRIT,
    STATUS_SKIP, /* check could not run on this system */
} status_t;

typedef struct {
    status_t status;
    char summary[SUMMARY_MAX];
    char detail[DETAIL_MAX]; /* newline-separated extra lines, may be empty */
} check_result_t;

typedef struct {
    double disk_warn_pct, disk_crit_pct;
    double mem_warn_pct, mem_crit_pct;
    double load_warn_per_cpu, load_crit_per_cpu;
} config_t;

typedef struct {
    const char *name;
    const char *description;
    void (*run)(const config_t *cfg, check_result_t *out);
} check_t;

typedef struct {
    const check_t *check;
    check_result_t result;
} run_entry_t;

/* registry.c */
extern const check_t CHECKS[];
extern const size_t CHECK_COUNT;

/* checks/ */
void check_disk(const config_t *cfg, check_result_t *out);
void check_memory(const config_t *cfg, check_result_t *out);
void check_load(const config_t *cfg, check_result_t *out);
void check_updates(const config_t *cfg, check_result_t *out);
void check_services(const config_t *cfg, check_result_t *out);
void check_reboot(const config_t *cfg, check_result_t *out);
void check_ports(const config_t *cfg, check_result_t *out);

/* Pure parsers, kept separate from I/O so they can be unit tested. */
int parse_meminfo(FILE *f, unsigned long *total_kb, unsigned long *avail_kb);
int parse_loadavg(const char *s, double load[3]);
double disk_used_pct(unsigned long long blocks, unsigned long long bfree,
                     unsigned long long bavail);
int parse_apt_check(const char *output, int *updates, int *security);
int count_upgradable(const char *apt_list_output);
int parse_failed_units(const char *output, check_result_t *out);
int parse_tcp_line(const char *line, unsigned *port, int *state, int *any_addr);

/* util.c */
status_t classify(double value, double warn, double crit);
int status_severity(status_t s);
const char *status_name(status_t s);
void result_set(check_result_t *r, status_t s, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void result_detail(check_result_t *r, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
int read_first_line(const char *path, char *buf, size_t size);
int run_command(const char *cmd, char *out, size_t outsz, int *exit_code);
void json_write_string(FILE *f, const char *s);
void json_write_stringn(FILE *f, const char *s, size_t len);

/* report.c */
status_t overall_status(const run_entry_t *entries, size_t n);
void report_text(FILE *f, const run_entry_t *entries, size_t n, int color, int verbose);
void report_json(FILE *f, const run_entry_t *entries, size_t n);

#endif
