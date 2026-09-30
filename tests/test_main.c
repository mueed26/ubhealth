/* Minimal self-contained test runner: no framework needed. */
#include "ubhealth.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int total, failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        total++;                                                             \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                    \
    } while (0)

static FILE *from_string(const char *s)
{
    return fmemopen((void *)s, strlen(s), "r");
}

static void test_classify(void)
{
    CHECK(classify(10, 80, 90) == STATUS_OK);
    CHECK(classify(80, 80, 90) == STATUS_WARN);
    CHECK(classify(95, 80, 90) == STATUS_CRIT);
    CHECK(status_severity(STATUS_CRIT) > status_severity(STATUS_WARN));
    CHECK(status_severity(STATUS_OK) > status_severity(STATUS_SKIP));
}

static void test_meminfo(void)
{
    unsigned long total_kb, avail_kb;
    FILE *f = from_string("MemTotal:       16000000 kB\n"
                          "MemFree:          100000 kB\n"
                          "MemAvailable:    4000000 kB\n");

    CHECK(parse_meminfo(f, &total_kb, &avail_kb) == 0);
    CHECK(total_kb == 16000000);
    CHECK(avail_kb == 4000000);
    fclose(f);

    f = from_string("MemTotal: 1000 kB\n"); /* old kernels lack MemAvailable */
    CHECK(parse_meminfo(f, &total_kb, &avail_kb) == -1);
    fclose(f);
}

static void test_loadavg(void)
{
    double l[3];

    CHECK(parse_loadavg("0.52 1.50 2.25 1/345 9999", l) == 0);
    CHECK(l[0] == 0.52 && l[1] == 1.50 && l[2] == 2.25);
    CHECK(parse_loadavg("garbage", l) == -1);
}

static void test_disk_pct(void)
{
    /* 100 blocks, 40 free, 30 available to users: 60 used of 90 usable */
    CHECK(fabs(disk_used_pct(100, 40, 30) - 66.666) < 0.01);
    CHECK(disk_used_pct(0, 0, 0) == 0.0);
    CHECK(disk_used_pct(10, 20, 5) == 0.0); /* nonsense input stays sane */
}

static void test_apt(void)
{
    int u = 0, s = 0;

    CHECK(parse_apt_check("12;3", &u, &s) == 0 && u == 12 && s == 3);
    CHECK(parse_apt_check("W: some warning\n5;0\n", &u, &s) == 0 && u == 5 && s == 0);
    CHECK(parse_apt_check("nothing useful", &u, &s) == -1);

    CHECK(count_upgradable("Listing... Done\n"
                           "curl/noble-updates 8.5.0-2ubuntu10.4 amd64 [upgradable from: 8.5.0-2ubuntu10.3]\n"
                           "vim/noble-updates 2:9.1 amd64 [upgradable from: 2:9.0]\n") == 2);
    CHECK(count_upgradable("Listing... Done\n") == 0);
}

static void test_failed_units(void)
{
    check_result_t r;

    memset(&r, 0, sizeof r);
    CHECK(parse_failed_units("foo.service loaded failed failed Foo daemon\n"
                             "bar.mount   loaded failed failed Bar\n", &r) == 2);
    CHECK(strcmp(r.detail, "foo.service\nbar.mount") == 0);
    CHECK(parse_failed_units("", NULL) == 0);
    CHECK(parse_failed_units("\n  \n", NULL) == 0);
}

static void test_tcp_line(void)
{
    unsigned port;
    int state, any;

    /* ssh on 0.0.0.0:22, listening */
    CHECK(parse_tcp_line("   0: 00000000:0016 00000000:0000 0A 00000000:00000000 00:00000000 "
                         "00000000     0        0 12345 1", &port, &state, &any) == 0);
    CHECK(port == 22 && state == 0x0A && any == 1);

    /* cups on 127.0.0.1:631: little-endian 0100007F is loopback */
    CHECK(parse_tcp_line("   1: 0100007F:0277 00000000:0000 0A 00000000:00000000",
                         &port, &state, &any) == 0);
    CHECK(port == 631 && any == 0);

    /* IPv6 [::]:80 */
    CHECK(parse_tcp_line("   0: 00000000000000000000000000000000:0050 "
                         "00000000000000000000000000000000:0000 0A 00000000:00000000",
                         &port, &state, &any) == 0);
    CHECK(port == 80 && any == 1);

    /* established connection, not listening */
    CHECK(parse_tcp_line("   2: 0F02000A:D2B4 5DB8D822:01BB 01 00000000:00000000",
                         &port, &state, &any) == 0);
    CHECK(state == 0x01 && any == 0);

    CHECK(parse_tcp_line("  sl  local_address rem_address   st tx_queue", &port, &state, &any) == -1);
    CHECK(parse_tcp_line("   0: 0000", &port, &state, &any) == -1);
    CHECK(parse_tcp_line("", &port, &state, &any) == -1);
}

static void test_result_detail_bounded(void)
{
    check_result_t r;

    memset(&r, 0, sizeof r);
    for (int i = 0; i < 1000; i++)
        result_detail(&r, "line %d with some padding text", i);
    CHECK(strlen(r.detail) < DETAIL_MAX);
}

static void test_json_escape(void)
{
    char *buf = NULL;
    size_t len = 0;
    FILE *f = open_memstream(&buf, &len);

    json_write_string(f, "a\"b\\c\n\x01");
    fclose(f);
    CHECK(strcmp(buf, "\"a\\\"b\\\\c\\n\\u0001\"") == 0);
    free(buf);
}

static void test_overall(void)
{
    run_entry_t e[3];

    memset(e, 0, sizeof e);
    e[0].result.status = STATUS_OK;
    e[1].result.status = STATUS_SKIP;
    e[2].result.status = STATUS_WARN;
    CHECK(overall_status(e, 3) == STATUS_WARN);
    CHECK(overall_status(e, 2) == STATUS_OK);
    CHECK(overall_status(&e[1], 1) == STATUS_SKIP);
}

int main(void)
{
    test_classify();
    test_meminfo();
    test_loadavg();
    test_disk_pct();
    test_apt();
    test_failed_units();
    test_tcp_line();
    test_result_detail_bounded();
    test_json_escape();
    test_overall();

    printf("%d/%d checks passed\n", total - failures, total);
    return failures ? 1 : 0;
}
