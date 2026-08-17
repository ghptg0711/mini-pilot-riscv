# mini-pilot-riscv (English)


> 简体中文文档：[README.zh-CN.md](README.zh-CN.md)
[![ci](https://github.com/ghptg0711/mini-pilot-riscv/actions/workflows/ci.yml/badge.svg)](https://github.com/ghptg0711/mini-pilot-riscv/actions/workflows/ci.yml)

A **mini telemetry collector for AI coding agents** written in C — the capability
demo for OSPP 2026 project [266eb0010 "LoongSuite Pilot RISC-V architecture
support"](https://summer.ospp.ac.cn). It normalizes agent session logs into the
GenAI event schema of [loongsuite-pilot](https://github.com/alibaba/loongsuite-pilot),
cross-compiles for **x86 and riscv64**, and runs under `qemu-riscv64` user mode.

## Features

| Feature | Detail | Version |
|---|---|---|
| GenAI event normalization | Required/Recommended fields per Pilot's `docs/output-event-schema.md` | v0.2 |
| riscv64 cross build | `riscv64-linux-gnu-gcc -static`, runs in qemu user mode | v0.3 |
| Test suite | expected diff / cross-arch byte-identical / schema assertions | v0.4 |
| Content masking | `--content` emits Opt-In fields; `--mask` replaces text with FNV-1a digest | v0.6 |
| Trace context | `observed_time_unix_nano` + W3C `trace_id`/`span_id` | v0.7 |
| Multi-agent | `--agent codex\|cursor\|qoder` field-alias normalization (four agents from the issue description) | v0.14 |
| Benchmark | `make bench`: throughput x86 vs qemu-riscv64 | v0.15 |
| Escaping safety | output-side JSON escaping + escape-aware input parsing | v0.9 |
| CI | GitHub Actions: dual-arch build + tests + riscv64 smoke | v0.10 |
| Ops surface | `mini-pilot status` (version/build arch/run arch), `host.arch` in events, end-to-end `scripts/smoke.sh` | v0.11 |

## Quick start

```bash
make all                 # build x86 + riscv64
./build/mini-pilot status        # ops-style status command
./build/mini-pilot --agent codex tests/fixtures/raw-codex.jsonl
./build/mini-pilot --agent qoder --provider qwen tests/fixtures/raw-qoder.jsonl
make test                # 6 assertions
make bench               # throughput: x86 vs qemu-riscv64
bash scripts/smoke.sh    # install-check -> build -> start -> collect -> verify
```

## Performance (make bench, 50k mixed-agent events)

| Runner | Throughput |
|---|---|
| x86 native | ~370,000 events/s |
| qemu-riscv64 (user mode, TCG) | ~22,600 events/s |

Numbers from the dev machine; the point is a **reproducible measurement
harness** for the riscv64 path, not absolute values.

## Design notes

- `src/jsonlite.[ch]`: zero-dependency JSON line parser for **flat objects with
  string/number/bool values** (controlled input); real projects should use a
  mature parser — this is a deliberate trade-off for static riscv64
  cross-compilation, declared in comments.
- Deterministic `event.id`/`trace_id` (time+seq / session hash) for
  reproducible tests; production should use UUIDs and the OTel SDK.
- The zero-native-dependency design itself mirrors the target project's
  "degradation strategy" requirement: the core path must stay usable when
  native modules (`sqlite3`, `zstd-napi`) are unavailable.

## License

Apache-2.0 (aligned with the upstream loongsuite-pilot project)
