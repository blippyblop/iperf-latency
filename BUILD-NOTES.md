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

## Windows — not built, why not

Attempted: Alpine `mingw-w64-gcc` 15.2 cross (`x86_64-w64-mingw32-*`) with
`--host=x86_64-w64-mingw32 CC=x86_64-w64-mingw32-gcc --with-openssl=no
LIBS=-lws2_32` plus POSIX→Winsock shim headers (sys/socket.h, netinet/in.h,
netinet/tcp.h, arpa/inet.h, netdb.h, sys/select.h, sys/uio.h, sys/resource.h,
sys/utsname.h, sys/mman.h, net/if.h, termios.h stub, `Windows.h`→`windows.h`).
That gets `configure` and most of the compile through, but the remaining
failures are **source-level** POSIX/Winsock mismatches in this master snapshot,
not shimmable:

- `uint` type used in GSO/GRO send paths (36×) — not provided by mingw headers.
- typed `setsockopt`/`getsockopt` argument-4 pointers (23×): Winsock declares
  `optval` as `const char*`; new options code (GSO/GRO, tcp_info, pacing,
  IPv6) passes typed structs with no WIN32 branch.
- `O_NONBLOCK`/`F_GETFL`/`F_SETFL` (no Winsock equivalent; needs
  `ioctlsocket(FIONBIO)`), `sigset_t`/`sigemptyset`/`sigaddset`, `MSG_TRUNC`,
  `getrusage`/`struct rusage` — all used without WIN32 fallbacks.

Upstream does not build or test Windows in CI (`.github/workflows/build.yml`
covers ubuntu/macos only), so the WIN32 paths in this snapshot are stale relative
to the post-3.21 features. Building a Windows binary would mean porting the
newer code (GSO/GRO, pacing, tcp_info, signal handling, getpass/termios) to
Winsock/winpthreads — out of scope here. A future Windows build is best done
against a release branch with maintained WIN32 support (or MSVC + the project's
historical mingw header-shim tooling) after the new features gain WIN32 branches.

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