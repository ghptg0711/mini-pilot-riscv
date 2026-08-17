# AGENTS.md

Notes for AI coding agents (and humans) working on this repository.
The layout intentionally mirrors [alibaba/loongsuite-pilot](https://github.com/alibaba/loongsuite-pilot)
so that conventions learned here transfer directly to the upstream project.

## Build & test

```bash
make all          # build x86 + riscv64 (static)
make test         # 4 assertions: expected diff / cross-arch identical / schema / codex
bash scripts/smoke.sh   # end-to-end: install-check -> build -> start -> collect -> verify
./build/mini-pilot status   # ops subcommand
```

Requires: `gcc`, `riscv64-linux-gnu-gcc`, `qemu-riscv64`, `python3`.
Reference environment manifest: `/home/gh/env/MANIFEST.md` (17/17 verified).

## Code map

| Path | Purpose (upstream analog) |
|---|---|
| `src/main.c` | pipeline orchestrator (index.ts + core/orchestrator) |
| `src/cli.c/h` | arg parsing + `status` subcommand (cli/) |
| `src/input.c/h` | agent-format alias normalization (inputs/) |
| `src/flusher.c/h` | GenAI event emission, schema mapping (flushers/) |
| `src/mask.c/h` | FNV-1a content fingerprinting (mask/) |
| `src/jsonlite.c/h` | zero-dependency JSON line parser (upstream uses jsonc-parser) |
| `tests/fixtures`, `tests/expected` | sample-driven contract tests (tests/) |

## Conventions

- Commits: conventional style with scope, e.g. `fix(jsonlite): ...`,
  `refactor(src): ...` (matches upstream history like `fix(dsh): ...`).
- Comments: English in code; docs are bilingual (README.md en +
  README.zh-CN.md), mirroring upstream docs/ + docs/zh-CN/.
- Schema source of truth: upstream `docs/output-event-schema.md`; any
  field-level change here must cite the schema level (Required /
  Recommended / Opt-In).
- Keep the riscv64 path first-class: every feature lands with a
  cross-arch assertion in `tests/run_test.sh` or `scripts/smoke.sh`.
- Teaching-grade trade-offs (deterministic ids, FNV-1a, flat-object
  parser) must be declared in code comments and README "Limitations".

## Limitations (do not silently "fix")

- jsonlite handles flat objects with string/number/bool only.
- `event.id` / `trace_id` are deterministic on purpose (reproducible
  tests). Do not randomize without updating the normalization rules in
  `tests/run_test.sh`.
