# Roadmap

Each task is sized to be one focused pull request.

## 1. `ports` check: exposed listening sockets (done in 0.1.0)

Implemented in `src/checks/ports.c` without calling `ss` or `netstat`, by
parsing the kernel's own tables. These notes explain how it works.

**How the data looks** (`cat /proc/net/tcp`):

```
  sl  local_address rem_address   st tx_queue rx_queue ...
   0: 00000000:0016 00000000:0000 0A 00000000:00000000 ...
   1: 0100007F:0277 00000000:0000 0A 00000000:00000000 ...
```

- `local_address` is `IP:PORT` in **hex**. The IPv4 address is in host
  (little-endian) byte order, so `0100007F` is `127.0.0.1`. The port is
  big-endian hex, so `0016` is port 22.
- `st` is the TCP state. `0A` is `LISTEN`.
- `/proc/net/tcp6` is the same, with a 32-hex-digit address.

**How it works**

1. `parse_tcp_line()` is a pure parser (unit tested with sample lines,
   including the header and truncated input).
2. `check_ports` reads both files and keeps LISTEN sockets bound to
   `0.0.0.0` or `::`. Loopback is ignored and duplicate ports are removed.
3. Ports are named with `getservbyport(3)` (for example 22 → ssh).

**Possible follow-ups:** allow listing "expected" ports (such as 22 on a
server) so they don't cause a warning, and cover UDP (`/proc/net/udp`).

## 2. Configurable thresholds

Add `--disk-warn 85` and similar options, or better, read
`~/.config/ubhealth/config` (a simple `key = value` file). Validate the
values: warn must be less than crit, and both must be between 0 and 100.

## 3. Man page

Write `ubhealth.1` in troff or scdoc. Install it from `make install`.

## 4. Strict snap confinement

Move from `devmode` to `strict`. Find out which interfaces are needed. Check
whether `apt-check` can run from inside the snap, and if not, find an
alternative such as reading `/var/lib/update-notifier/updates-available`.
Write up what you learn. Understanding snap confinement is directly relevant
to Canonical.

## 5. Swap and inode checks

Inode exhaustion (`statvfs().f_files` / `f_ffree`) is a classic failure that
`df -h` doesn't show.

## 6. Debian packaging

Add a `debian/` directory and build a `.deb` with `dpkg-buildpackage`, then
publish it through a Launchpad PPA.
