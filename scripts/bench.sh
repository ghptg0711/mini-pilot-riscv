#!/bin/bash
# Throughput benchmark: x86 native vs qemu-riscv64 user-mode.
# Generates a large mixed-agent session log, then measures events/s.
# Usage: bash scripts/bench.sh [num_lines]   (default 50000)
set -e
cd "$(dirname "$0")/.."
N=${1:-50000}
FIX=/tmp/bench-session.jsonl
python3 - "$N" > "$FIX" <<'EOF'
import sys, json, random
random.seed(42)
n = int(sys.argv[1])
agents = [
    ("claude-code", {"ts": "2026-08-17T%02d:%02d:%02dZ", "type": "request",  "model": "claude-sonnet-4", "text": "bench payload %d", "session": "s-%d"}),
]
# interleave the four native formats to exercise the alias table
for i in range(n):
    t = i % 86400
    hh, mm, ss = t // 3600, (t // 60) % 60, t % 60
    k = i % 4
    if k == 0:
        l = {"ts": f"2026-08-17T{hh:02d}:{mm:02d}:{ss:02d}Z", "type": "request", "model": "claude-sonnet-4", "text": f"bench payload {i}", "session": f"s-{i%97}"}
    elif k == 1:
        l = {"timestamp": f"2026-08-17T{hh:02d}:{mm:02d}:{ss:02d}Z", "event_type": "model_response", "model": "gpt-5-codex", "input_tokens": 100 + i % 500, "output_tokens": 50 + i % 300, "reply": "ok", "conversation_id": f"cx-{i%89}"}
    elif k == 2:
        l = {"time": f"2026-08-17T{hh:02d}:{mm:02d}:{ss:02d}Z", "kind": "ai.request", "model": "claude-sonnet-4", "prompt": f"bench {i}", "conversationId": f"cur-{i%83}"}
    else:
        l = {"ts": f"2026-08-17T{hh:02d}:{mm:02d}:{ss:02d}Z", "event": "response", "model": "qwen3-coder", "in_tok": 80 + i % 400, "out_tok": 40 + i % 200, "content": "好", "conv_id": f"q-{i%79}"}
    print(json.dumps(l, ensure_ascii=False, separators=(",", ":")))
EOF
echo "fixture: $N lines ($(du -h $FIX | cut -f1))"

bench(){ # bench <label> <cmd...>
  local label=$1; shift
  local s=$(date +%s.%N)
  "$@" > /tmp/bench-out.jsonl
  local e=$(date +%s.%N)
  local eps=$(python3 -c "n=$N; print(f'{n/($e-$s):,.0f}')")
  local got=$(wc -l < /tmp/bench-out.jsonl)
  echo "$label: $got events in $(python3 -c "print(f'{$e-$s:.2f}')")s = $eps events/s"
}
bench "x86-native " ./build/mini-pilot "$FIX"
bench "qemu-riscv64" qemu-riscv64 ./build/mini-pilot-riscv64 "$FIX"
echo "校验: $(python3 -c "import json; ls=[json.loads(l) for l in open('/tmp/bench-out.jsonl')]; print(len(ls), 'events, all valid JSON')")"
