#include "ubhealth.h"

#include <stdarg.h>
#include <string.h>
#include <sys/wait.h>

status_t classify(double value, double warn, double crit)
{
    if (value >= crit)
        return STATUS_CRIT;
    if (value >= warn)
        return STATUS_WARN;
    return STATUS_OK;
}

/* SKIP ranks lowest so a check that cannot run never masks a real result. */
int status_severity(status_t s)
{
    switch (s) {
    case STATUS_SKIP: return 0;
    case STATUS_OK:   return 1;
    case STATUS_WARN: return 2;
    case STATUS_CRIT: return 3;
    }
    return 0;
}

const char *status_name(status_t s)
{
    switch (s) {
    case STATUS_OK:   return "ok";
    case STATUS_WARN: return "warn";
    case STATUS_CRIT: return "crit";
    case STATUS_SKIP: return "skip";
    }
    return "unknown";
}

void result_set(check_result_t *r, status_t s, const char *fmt, ...)
{
    va_list ap;

    r->status = s;
    va_start(ap, fmt);
    vsnprintf(r->summary, sizeof r->summary, fmt, ap);
    va_end(ap);
}

void result_detail(check_result_t *r, const char *fmt, ...)
{
    size_t len = strlen(r->detail);
    va_list ap;

    if (len >= DETAIL_MAX - 2)
        return; /* full: drop further lines rather than overflow */
    if (len > 0)
        r->detail[len++] = '\n';
    va_start(ap, fmt);
    vsnprintf(r->detail + len, DETAIL_MAX - len, fmt, ap);
    va_end(ap);
}

int read_first_line(const char *path, char *buf, size_t size)
{
    FILE *f = fopen(path, "r");

    if (!f)
        return -1;
    if (!fgets(buf, (int)size, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

/*
 * Run a shell command and capture stdout into out (truncated to outsz - 1).
 * Returns -1 if the command could not be started at all.
 */
int run_command(const char *cmd, char *out, size_t outsz, int *exit_code)
{
    char chunk[512];
    size_t len = 0, got;
    FILE *p = popen(cmd, "r");
    int st;

    if (!p)
        return -1;
    while ((got = fread(chunk, 1, sizeof chunk, p)) > 0) {
        if (outsz > 0 && len < outsz - 1) {
            size_t room = outsz - 1 - len;
            size_t take = got < room ? got : room;

            memcpy(out + len, chunk, take);
            len += take;
        }
        /* keep draining so the child never blocks on a full pipe */
    }
    if (outsz > 0)
        out[len] = '\0';
    st = pclose(p);
    if (exit_code)
        *exit_code = (st != -1 && WIFEXITED(st)) ? WEXITSTATUS(st) : -1;
    return 0;
}

void json_write_stringn(FILE *f, const char *s, size_t len)
{
    fputc('"', f);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];

        switch (c) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f); break;
        case '\r': fputs("\\r", f); break;
        case '\t': fputs("\\t", f); break;
        default:
            if (c < 0x20)
                fprintf(f, "\\u%04x", c);
            else
                fputc(c, f);
        }
    }
    fputc('"', f);
}

void json_write_string(FILE *f, const char *s)
{
    json_write_stringn(f, s, strlen(s));
}
