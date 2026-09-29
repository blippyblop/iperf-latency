# Build notes — `--measure-latency` (iperf 3.21+ @ c9b7422)

Artifacts (static, self-contained):

| Target    | Path                          | Status |
|-----------|-------------------------------|--------|
| x86_64 (musl)  | `bin/x86_64/iperf-latency`   | built, tested end-to-end (localhost UDP: normal, `-R`, `-P 2`, `-J`, disabled, 2 s short test) |
| aarch64 (musl) | `bin/aarch64/iperf-latency`  | built, runs + full `--measure-latency` exchange verified under `qemu-aarch64` |
| Windows x86_64 | —                             | **not feasible** on this snapshot, see below |

Source tree: this repository (upstream esnet/iperf master @ c9b7422, with the
feature added and the binary renamed from `iperf3` to `iperf-latency`).
Prebuilt static binaries are in `bin/<arch>/iperf-latency`; `design.md` is the
original feature spec.

> **2026-09-29:** binaries are no longer committed to the repository. They were
> removed from git tracking and `bin/` is gitignored — it only holds local build
> output now. Release binaries (linux x86_64, linux aarch64, windows x86_64) are
> built and attached to GitHub releases automatically by
> `.github/workflows/release.yml` on release publish. The Linux builds there
> reproduce the recipes below with musl (static, `--with-openssl=no`,
> `--enable-static-bin`). The Windows attempt in that workflow is best-effort
> (`continue-on-error`) until the source-level WIN32 gaps described below are
> fixed. Note: the previously committed binaries predate the clock-domain
> latency fix (commit 306d671) and are stale.
Feature: `--measure-latency` (client-only, UDP only). NTP-style 4-timestamp clock
sync over the TCP control channel at ~1 Hz (`CLOCK_SYNC_REQ 17` / `CLOCK_SYNC_RSP 18`,
signed char + 2×BE64 µs), latency of real UDP data packets = transit time corrected
by the per-direction clock offset, stats = min/mean/median/p95/p99/max/stdev,
per-interval (receiver side) + whole-test, human (`PACKET LATENCY` block) and JSON
(`latency_*_ms`, `latency_samples`, top-level `latency_measurement` object).
Disabled mode changes no protocol, output, or behavior.

## x86_64 (native)

```
./configure --with-openssl=no && make
```
(plain `./configure` is equivalent on a box without OpenSSL headers.)
Static artifact: `gcc -O2 -static src/iperf_latency-main.o src/.libs/libiperf.a -lpthread`.

## aarch64 (cross from x86_64)

Alpine 3.24 has no aarch64 cross-compiler package; used the prebuilt musl cross
toolchain from musl.cc:

```
curl -O https://musl.cc/aarch64-linux-musl-cross.tgz && tar xzf <it>
cp -r <source> build/ && make -C build distclean
cd build
PATH=.../aarch64-linux-musl-cross/bin:$PATH \
  ./configure --host=aarch64-alpine-linux-musl CC=aarch64-linux-musl-gcc --with-openssl=no
PATH=.../aarch64-linux-musl-cross/bin:$PATH make -j$(nproc)
# static single file:
aarch64-linux-musl-gcc -O2 -static src/iperf_latency-main.o src/.libs/libiperf.a -lpthread -o iperf-latency-aarch64
```
Verified under `qemu-aarch64` (server+client on loopback). Note: under qemu-user
emulation the two emulated processes wake asymmetrically on the host CPU, which
biases the NTP offset estimate by ~±2 ms (delay asymmetry, not a protocol bug —
both sides use the same host clock, so the true offset is 0); the latency
distribution shape (stdev, min→max spread) is unaffected. On real aarch64
hardware expect the same ~±25 µs offset error as the x86_64 runs.

## Windows — built since 2026-09-29 (mingw-w64 port)

Earlier attempts failed on source-level POSIX/Winsock mismatches (bare `uint`,
typed `setsockopt` optval pointers, `O_NONBLOCK`/`fcntl`, POSIX signals,
`getrusage`, `MSG_TRUNC`, `strsignal`/`kill`, an always-on `mmap`'d send
buffer, missing `uname`, etc.). Those are all ported now:

* `src/windows/` — POSIX-name shim headers (sys/socket.h, netdb.h, poll.h, …)
  selected with `CPPFLAGS="-Isrc/windows"`.
* `src/iperf_win_compat.h` — included by `iperf.h` on Windows: winsock2-first
  include order, `close()`→`closesocket()`, Winsock→errno translation
  (`SOCK_ERRNO`), uniform `iperf_setsockopt`/`iperf_getsockopt` wrappers
  (Winsock wants `char*` optval), `poll()` via `WSAPoll`, `strsignal`
  fallback, `BYTE_ORDER`, `FD_SETSIZE` bump.
* Source fixes (all `#ifdef`'d, POSIX behavior unchanged): send buffer uses
  `malloc` instead of `mkstemp`+`mmap`; `setnonblocking`/`timeout_connect`
  use `ioctlsocket(FIONBIO)`; signal-blocking and `SIGPIPE` handling skipped;
  `cpu_util` via `GetProcessTimes`; `iperf_getpass` via console mode;
  `daemon()` unsupported (returns -1); pidfile liveness via `OpenProcess`;
  `SO_RCVTIMEO` (Winsock units differ) not set; `TCP_MAXSEG` guarded;
  `portable_endian.h` uses `__builtin_bswap64` for `htobe64`/`be64toh`.
* CI: `.github/workflows/release.yml` builds it on a native `windows-latest`
  runner (MSYS2 mingw-w64) with
  `./configure --with-openssl=no --enable-static-bin CPPFLAGS="-D_WIN32_WINNT=0x0601 -Isrc/windows" LDFLAGS=-static LIBS=-lws2_32`
  and runs the localhost `--measure-latency` smoke test on real Windows.

Known Windows differences: GSO/GRO, TCP_INFO stats, socket pacing, CPU
affinity (Linux mechanisms) are off; `-D/--daemon` is rejected; `--logfile`
and `-F` diskfile modes use text-mode translation unless opened binary.
Widely used paths (TCP/UDP throughput, `-R`, `-P`, `--measure-latency`,
JSON) are covered by the CI smoke test.

### Historical note — why it was not built before

Attempted: Alpine `mingw-w64-gcc` 15.2 cross (`x86_64-w64-mingw32-*`) with
`--host=x86_64-w64-mingw32 CC=x86_64-w64-mingw32-gcc --with-openssl=no
LIBS=-lws2_32` plus POSIX→Winsock shim headers (sys/socket.h, netinet/in.h,
netinet/tcp.h, arpa/inet.h, netdb.h, sys/select.h, sys/uio.h, sys/resource.h,
sys/utsname.h, sys/mman.h, net/if.h, termios.h stub, `Windows.h`→`windows.h`).
That gets `configure` and most of the compile through, but the remaining
failures are **source-level** POSIX/Winsock mismatches in this master snapshot,
not shimmable: `uint` in GSO/GRO paths, typed `setsockopt`/`getsockopt`
argument-4 pointers, `O_NONBLOCK`/`F_GETFL`/`F_SETFL`, `sigset_t`,
`MSG_TRUNC`, `getrusage` — all used without WIN32 fallbacks. (All of these
have since been fixed; see above.)

## Test evidence (x86_64 localhost, 50 Mbit/s UDP)

- normal: per-stream line + `PACKET LATENCY` block, e.g. min 0.041 / avg 0.095 /
  med 0.089 / p95 0.174 / p99 0.244 / max 0.305 ms (true loopback values)
- `-J`: `udp.latency_*_ms` + `latency_samples` per stream, top-level
  `latency_measurement` {enabled, clock_sync_samples, clock_sync_errors,
  clock_offset_ms, min_sync_rtt_ms, offset_stdev_ms}
- `-R`: client-side (receiver) latency, sane values
- `-P 2`: independent per-stream `PACKET LATENCY` blocks
- `--get-server-output` (+`-J`): server-side human latency block in
  `server_output_text`
- disabled (no flag): output/protocol identical to stock
- errors: TCP test → "--measure-latency is only supported for UDP tests";
  server use → "client only" option error
- known quirk: the client main loop wakes on the 1 s stats timer, so sync
  cadence is ~1 s with occasional 2 s gaps (spec: "roughly once per second");
  tests shorter than ~2 s still yield receiver-side samples from t≈1 s onward