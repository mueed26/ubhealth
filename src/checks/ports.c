#include "ubhealth.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#define TCP_LISTEN 0x0A /* from include/net/tcp_states.h in the kernel */
#define MAX_PORTS 256

/*
 * Parse one line of /proc/net/tcp or /proc/net/tcp6:
 *
 *   0: 00000000:0016 00000000:0000 0A 00000000:00000000 ...
 *      local addr:port  remote       state
 *
 * The address is hex (8 digits for IPv4, 32 for IPv6); the port is hex.
 * Sets *any_addr when the socket is bound to 0.0.0.0 or ::, i.e. reachable
 * on every interface. Returns -1 for the header line or malformed input.
 */
int parse_tcp_line(const char *line, unsigned *port, int *state, int *any_addr)
{
    char addr[64];
    unsigned p, st;
    size_t len;

    if (sscanf(line, " %*d: %63[0-9A-Fa-f]:%x %*[0-9A-Fa-f]:%*x %x", addr, &p, &st) != 3)
        return -1;
    len = strlen(addr);
    if ((len != 8 && len != 32) || p > 65535)
        return -1;
    *port = p;
    *state = (int)st;
    *any_addr = strspn(addr, "0") == len;
    return 0;
}

static int cmp_unsigned(const void *a, const void *b)
{
    unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;

    return (x > y) - (x < y);
}

/* Append exposed listening ports from one /proc table, skipping duplicates. */
static int collect_ports(const char *path, unsigned ports[], size_t *n)
{
    char line[512];
    FILE *f = fopen(path, "r");

    if (!f)
        return -1;
    while (fgets(line, sizeof line, f)) {
        unsigned port;
        int state, any, dup = 0;

        if (parse_tcp_line(line, &port, &state, &any) != 0)
            continue;
        if (state != TCP_LISTEN || !any)
            continue;
        for (size_t i = 0; i < *n; i++)
            if (ports[i] == port)
                dup = 1;
        if (!dup && *n < MAX_PORTS)
            ports[(*n)++] = port;
    }
    fclose(f);
    return 0;
}

void check_ports(const config_t *cfg, check_result_t *out)
{
    unsigned ports[MAX_PORTS];
    size_t n = 0, used = 0;
    char list[128] = "";
    int have4, have6;

    (void)cfg;
    have4 = collect_ports("/proc/net/tcp", ports, &n) == 0;
    have6 = collect_ports("/proc/net/tcp6", ports, &n) == 0;
    if (!have4 && !have6) {
        result_set(out, STATUS_SKIP, "Cannot read /proc/net/tcp");
        return;
    }
    if (n == 0) {
        result_set(out, STATUS_OK, "No TCP ports open to the network");
        return;
    }
    qsort(ports, n, sizeof ports[0], cmp_unsigned);

    for (size_t i = 0; i < n; i++) {
        struct servent *se = getservbyport(htons((unsigned short)ports[i]), "tcp");
        int w;

        result_detail(out, "%5u/tcp  %s", ports[i], se ? se->s_name : "unknown service");
        if (used < sizeof list) {
            w = snprintf(list + used, sizeof list - used, "%s%u", i ? ", " : "", ports[i]);
            used += w > 0 ? (size_t)w : 0;
        }
    }
    if (used >= sizeof list) /* list got truncated */
        snprintf(list + sizeof list - 5, 5, "...");
    endservent();
    result_set(out, STATUS_WARN, "%zu port(s) open to the network: %s", n, list);
}
