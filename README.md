<h1 align="center">ubhealth</h1>

<p align="center">
  <b>One command to answer "is this Ubuntu machine OK?"</b><br>
  A fast, dependency-free system health checker written in C.
</p>

<p align="center">
  <a href="https://github.com/mueed26/ubhealth/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/mueed26/ubhealth/actions/workflows/ci.yml/badge.svg"></a>
  <img alt="Language: C11" src="https://img.shields.io/badge/language-C11-blue">
  <img alt="Platform: Ubuntu / Linux" src="https://img.shields.io/badge/platform-Ubuntu%20%7C%20Linux-E95420">
  <img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-green">
</p>

---

`ubhealth` checks disk space, memory, CPU load, pending security updates,
failed systemd services, pending reboots and network-exposed ports, then gives
you a single colour-coded report, or JSON for scripts and monitoring.

Real output from an Ubuntu 24.04 machine:

```text
$ ubhealth
ubhealth 0.1.0 - system health report for Mir

  [ OK ] disk      1 filesystem(s) healthy, fullest is / at 2%
  [ OK ] memory    8% of RAM in use (0.6 / 7.4 GiB)
  [ OK ] load      Load 0.02 0.14 0.26 across 16 CPU(s)
  [WARN] updates   62 update(s) pending
                   Run: sudo apt update && sudo apt upgrade
  [ OK ] services  No failed systemd units
  [ OK ] reboot    No reboot required
  [WARN] ports     1 port(s) open to the network: 22
                      22/tcp  ssh

  5 ok, 2 warning, 0 critical, 0 skipped
```

## Contents

- [Why](#why)
- [Features](#features)
- [Quick start](#quick-start)
- [Installation](#installation)
- [Usage](#usage)
- [The checks](#the-checks)
- [How it works](#how-it-works)
- [Development](#development)
- [Project structure](#project-structure)
- [Roadmap](#roadmap)
- [License](#license)

## Why

To answer "does this machine need attention?" you normally run `df -h`, `free`,
`uptime`, `systemctl --failed`, `apt list --upgradable` and `ss -tlnp`, check
for `/var/run/reboot-required`, and then interpret all the output yourself.

`ubhealth` does all of that in one step and gives:

- a **human** answer: a colour report that shows only the details that matter
- a **machine** answer: JSON output plus meaningful exit codes, so it works
  in cron jobs, CI pipelines, SSH loops and monitoring tools

## Features

- **7 checks**: disk, memory, load, updates, services, reboot and ports
- **Zero dependencies.** Plain C11 and libc. Reads `/proc` and uses syscalls
  directly instead of running `df`, `free` or `ss`
- **Fast.** Starts instantly and uses almost no memory
- **Script-friendly.** `--json` output and exit codes `0`/`1`/`2`
- **Works in containers and WSL.** Checks that can't run report `SKIP`
  instead of failing
- **Tested.** 40 unit tests covering every parser, with CI on gcc and clang
  and runs under AddressSanitizer and UndefinedBehaviorSanitizer
- **Snap packaging** (experimental, in devmode)

## Quick start

```bash
sudo apt install -y build-essential git
git clone https://github.com/mueed26/ubhealth.git
cd ubhealth
make
./ubhealth
```

## Installation

### From source

```bash
sudo apt install -y build-essential   # gcc + make
make                                  # builds ./ubhealth
make test                             # optional: run the unit tests
sudo make install                     # installs to /usr/local/bin
```

To install somewhere else: `make install PREFIX=$HOME/.local`.
To uninstall: `sudo rm /usr/local/bin/ubhealth`.

### As a snap (experimental)

```bash
sudo snap install snapcraft --classic
snapcraft
sudo snap install --devmode ubhealth_0.1.0_*.snap
```

The snap runs in `devmode` (without the full sandbox) for now. Moving it to
strict confinement is on the [roadmap](ROADMAP.md).

## Usage

```text
ubhealth [OPTIONS]
```

| Option | Description |
|---|---|
| `-j`, `--json` | Machine-readable JSON output |
| `-o`, `--only LIST` | Run only these checks, comma separated (e.g. `disk,ports`) |
| `-l`, `--list` | List available checks and exit |
| `-v`, `--verbose` | Show details for every check, not only for problems |
| `-n`, `--no-color` | Disable colours (the `NO_COLOR` environment variable also works) |
| `-h`, `--help` | Show help |
| `-V`, `--version` | Show version |

### Exit codes

| Code | Meaning |
|---|---|
| `0` | Everything OK |
| `1` | At least one warning |
| `2` | At least one critical problem |
| `3` | Invalid command-line usage |

Checks that report `SKIP` never affect the exit code.

### Examples

Alert when a machine needs attention:

```bash
ubhealth --only disk,updates || notify-send "Machine needs attention"
```

Check several servers over SSH:

```bash
for h in web-01 web-02 db-01; do
  echo "$h: $(ssh "$h" ubhealth --json | jq -r .overall)"
done
```

Log a health snapshot every hour with cron:

```cron
0 * * * * /usr/local/bin/ubhealth --json >> /var/log/ubhealth.jsonl
```

### JSON output

Real output of `ubhealth --json --only disk,ports`:

```json
{
  "version": "0.1.0",
  "hostname": "Mir",
  "overall": "warn",
  "checks": [
    {"name": "disk", "status": "ok", "summary": "1 filesystem(s) healthy, fullest is / at 2%", "details": ["/                          2.3% used  (/dev/sdd)"]},
    {"name": "ports", "status": "warn", "summary": "1 port(s) open to the network: 22", "details": ["   22/tcp  ssh"]}
  ]
}
```

## The checks

| Check | Data source | WARN | CRIT |
|---|---|---|---|
| `disk` | `/proc/mounts` + `statvfs(3)` for ext4, xfs, btrfs, zfs, f2fs and vfat | ≥ 80% used | ≥ 90% used |
| `memory` | `MemTotal` / `MemAvailable` in `/proc/meminfo` | ≥ 85% used | ≥ 95% used |
| `load` | 5-minute average in `/proc/loadavg` ÷ number of CPUs | ≥ 1.0 per CPU | ≥ 2.0 per CPU |
| `updates` | `/usr/lib/update-notifier/apt-check`, falling back to `apt list --upgradable` | any update pending | any security update pending |
| `services` | `systemctl --failed` | any failed unit | none |
| `reboot` | `/var/run/reboot-required` and `.pkgs` | reboot pending | none |
| `ports` | `/proc/net/tcp` and `/proc/net/tcp6` | TCP port listening on `0.0.0.0` or `::` | none |

Some details worth knowing:

- **Disk usage matches `df`.** Blocks reserved for root count as neither used
  nor free. Snap `squashfs` images, `tmpfs` and network mounts are ignored.
  A device mounted in several places (a bind mount) is counted once.
- **Memory uses `MemAvailable`, not `MemFree`.** Linux uses spare RAM for disk
  cache, which it can reclaim at any time, so `MemFree` makes a healthy
  machine look full.
- **Load is divided by the number of cores.** A load of 4 is heavy on a
  2-core laptop but light on a 32-core server.
- **Ports decodes the kernel's socket table directly.** For example
  `0100007F:0277` is `127.0.0.1:631`, because IPv4 addresses are stored in
  little-endian order. Only sockets reachable from the network are reported;
  loopback-only services are ignored.

## How it works

```text
main.c ──► parses options ──► for each selected check in registry.c
                                   │
                                   ▼
                     check_xxx(config, &result)      src/checks/*.c
                        │  read /proc, syscalls, commands
                        ▼
                     pure parser  ◄── unit tests feed fixture text here
                        │
                        ▼
                     result: status + summary + details
                                   │
                                   ▼
                     report.c ──► coloured text or JSON ──► exit code
```

Design principles:

1. **Each check is a plug-in.** A check is one function with the signature
   `void check_x(const config_t *, check_result_t *)`. Adding a check means
   adding one file and one line in `src/registry.c`.
2. **Parsing is separate from I/O.** Every parser takes a string or a
   `FILE *`, so tests can check the logic using sample text, without root
   access or a particular machine.
3. **Fail soft.** A check that can't run returns `SKIP` with a reason. It
   never crashes or aborts the whole report.
4. **Bounded memory.** Fixed-size result buffers and truncation-safe string
   building mean no unbounded allocations, which the sanitizers verify.

## Development

### Prerequisites

- Ubuntu 22.04 or later (or WSL2 with Ubuntu), or any recent Linux
- `build-essential`; `clang` is optional

### Common tasks

```bash
make                       # build
make test                  # run the 40 unit tests
make asan                  # rebuild + test under AddressSanitizer and UBSan
make CC=clang              # build with clang
make CFLAGS="-O2 -Werror"  # treat warnings as errors (what CI does)
make clean
```

These compiler warnings are always enabled: `-Wall -Wextra -Wpedantic
-Wshadow -Wformat=2 -Wstrict-prototypes`.

### Continuous integration

GitHub Actions ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) runs
the following on every push and pull request, with both gcc and clang:
a build with `-Werror`, the unit tests, the tests under both sanitizers, a
full test run, and a JSON validity check.

### Adding a new check

1. Create `src/checks/mycheck.c` with `void check_mycheck(const config_t *cfg, check_result_t *out)`.
2. Put any parsing in a separate pure function and add tests to `tests/test_main.c`.
3. Declare both functions in `include/ubhealth.h`.
4. Add `{ "mycheck", "Description", check_mycheck }` to `src/registry.c`.
5. Run `make test && make asan`.

## Project structure

```text
ubhealth/
├── include/ubhealth.h      shared types and function declarations
├── src/
│   ├── main.c              command-line parsing, runs checks, exit code
│   ├── registry.c          list of checks, in report order
│   ├── report.c            text and JSON output
│   ├── util.c              thresholds, safe string building, popen wrapper, JSON escaping
│   └── checks/
│       ├── disk.c          filesystem usage
│       ├── memory.c        RAM usage
│       ├── load.c          CPU load per core
│       ├── updates.c       apt and security updates
│       ├── services.c      failed systemd units
│       ├── reboot.c        pending reboot
│       └── ports.c         network-exposed TCP ports
├── tests/test_main.c       unit tests
├── snap/snapcraft.yaml     snap packaging
├── .github/workflows/      CI pipeline
├── Makefile
└── ROADMAP.md
```

## Roadmap

Planned work is in [ROADMAP.md](ROADMAP.md), including configurable
thresholds, a man page, strict snap confinement, inode checks and Debian
packaging.

## License

[MIT](LICENSE) © 2026 Mueed Hyder
