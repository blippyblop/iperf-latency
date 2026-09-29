iperf3 UDP Packet Latency Measurement
Goal

Add an opt-in iperf3 feature that measures the end-to-end latency of the actual UDP test packets.

Primary use case:

iperf3 -R -u -b 50M ... → 500M


We want to determine whether high offered load causes packet delivery latency to increase, even when UDP packet loss remains low.

Do not use ICMP/ping. Measure the actual iperf3 UDP traffic.

Core Design
1. Use existing UDP timestamps

iperf3 UDP packets already contain:

sequence number

sender timestamp

The receiver already calculates sender/receiver timestamp differences for jitter.

Do not redesign the UDP packet format unless necessary.

2. Add optional clock synchronization

Add an opt-in flag:

--measure-latency


Use iperf3's existing TCP control connection for synchronization. Do not send synchronization packets as part of the UDP test stream.

Approximately once per second, perform an NTP-style four-timestamp exchange:

Server                         Client

T1 ───── sync request ────────► T2
                               T3
    ◄──── sync response ────── T4


Calculate:

RTT = (T4 - T1) - (T3 - T2)

clock_offset =
    ((T2 - T1) + (T3 - T4)) / 2


Define the offset consistently as:

client_clock - server_clock


Maintain/update the offset throughout the test to account for clock drift.

Prefer low-RTT synchronization samples when determining the clock offset; delayed sync packets may be queued under load and should not corrupt the offset estimate.

Record sync quality:

number of sync samples

current/selected clock offset

minimum sync RTT

offset variation if practical

3. Calculate actual UDP packet latency

For every received UDP packet when --measure-latency is enabled:

corrected_send_time = sender_timestamp + clock_offset

latency =
    receiver_arrival_time - corrected_send_time


Record latency samples.

This measures:

iperf3 sender
→ OS/network stack
→ Wi-Fi/network
→ receiver OS/network stack
→ iperf3 receiver


It is intentionally end-to-end application-visible packet delay, not pure 802.11 airtime.

4. Calculate latency statistics

Calculate latency statistics both:

per reporting interval

for the complete test

At minimum:

min
mean
median / P50
P95
P99
max
standard deviation


Existing jitter/loss/throughput calculations must remain unchanged.

Latency and jitter are separate metrics:

latency = absolute packet delivery delay

jitter = variation in packet delivery timing

5. JSON output

When --measure-latency is enabled, add latency data to the existing JSON.

Conceptually:

"latency_ms": {
    "min": 4.7,
    "mean": 6.2,
    "median": 5.3,
    "p95": 11.4,
    "p99": 24.7,
    "max": 48.2,
    "stdev": 5.1
}


Include equivalent per-interval statistics.

Also include synchronization information, e.g.:

"latency_measurement": {
    "enabled": true,
    "clock_sync_samples": 30,
    "clock_offset_ms": 1.203,
    "min_sync_rtt_ms": 0.72
}


Exact JSON structure may follow existing iperf3 conventions.

6. Human-readable output

Add a section:

PACKET LATENCY
------------------------------------------------------------
Minimum:            4.700 ms
Average:            6.200 ms
Median:             5.300 ms
Std deviation:      5.100 ms
P95:               11.400 ms
P99:               24.700 ms
Maximum:            48.200 ms


Add P99 latency to the overall ramp summary.

Important Constraints
Do not change the benchmark traffic

With latency measurement disabled, iperf3 behavior must remain unchanged.

With latency measurement enabled:

UDP bitrate must remain unchanged.

UDP packet generation must not block waiting for clock synchronization.

Synchronization uses the existing control connection.

Failed/delayed synchronization must not stop the UDP test.

Do not add ping/ICMP traffic.

The feature is for measurement, not traffic modification.

-R Support

Must work correctly with reverse mode:

iperf3 -c SERVER -R -u --measure-latency


In this mode:

SERVER ───── UDP ─────► CLIENT


The sender timestamp is in server clock time and must be converted into client clock time using the synchronized offset.

Normal non--R UDP must also work.

TCP

Do not implement TCP packet latency in the first version.

TCP write()/read() boundaries do not correspond cleanly to individual network packets.

Keep this feature focused on UDP.

Existing TCP TCP_INFO RTT/retransmission statistics should remain untouched.

Suggested Source Areas

Likely primary areas:

src/iperf_udp.c
src/iperf_udp.h
src/iperf.h
src/iperf_api.c


Use the existing iperf3 control-channel protocol for synchronization.

Reuse iperf3's existing time abstraction rather than introducing unrelated timestamp mechanisms.

Implementation Order

Add --measure-latency.

Add control-channel clock synchronization.

Maintain synchronized clock offset.

Use existing UDP sender timestamps + receiver arrival timestamps.

Calculate corrected per-packet latency.

Aggregate per-interval and whole-test statistics.

Add JSON output.

Add human-readable output.

Add/modify tests for:

localhost

wired Ethernet

normal UDP

-R UDP

synchronization failure/delay

Verify that latency measurement does not materially alter UDP throughput behavior.

Success Criterion

Running:

iperf3 -c SERVER -R -u -b 500M -t 30 --measure-latency -J


must produce throughput, loss, jitter and the latency distribution of the actual UDP packets, allowing a ramp such as 50–500 Mbps to reveal whether increasing load causes packet delivery latency to rise even when reported packet loss remains low.
