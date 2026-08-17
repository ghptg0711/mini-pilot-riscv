#!/bin/bash
# 双平台一致性测试：x86 与 riscv64(qemu) 输出（剔除不稳定字段后）应一致且符合期望
set -e
cd "$(dirname "$0")/.."
FIX=tests/fixtures/raw-claude.jsonl
EXP=tests/expected/normalized.jsonl
norm(){ sed -E 's/"event.id": "[^"]*"/"event.id": "<ID>"/; s/"observed_time_unix_nano": [0-9]+/"observed_time_unix_nano": <T>"/; s/"host.name": "[^"]*"/"host.name": "<HOST>"/'; }

./build/mini-pilot            "$FIX" | norm > /tmp/out-x86.jsonl
./build/mini-pilot            "$FIX"         > /tmp/raw-x86.jsonl
qemu-riscv64 ./build/mini-pilot-riscv64 "$FIX" | norm > /tmp/out-riscv.jsonl

pass=0
diff /tmp/out-x86.jsonl   "$EXP" && { echo "PASS x86   输出与期望一致"; pass=$((pass+1)); }
diff /tmp/out-x86.jsonl /tmp/out-riscv.jsonl && { echo "PASS 跨架构 x86==riscv64 输出一致"; pass=$((pass+1)); }
# schema 断言跑在未做占位符归一化的真实输出上（占位符不是合法 JSON）
python3 -c "import json
for l in open('/tmp/raw-x86.jsonl'):
    e=json.loads(l)
    assert e['time_unix_nano']>0 and e['event.name'] and e['user.id'] and e['gen_ai.agent.type']
print('PASS 输出满足 schema Required 字段')"; pass=$((pass+1))
# v0.8: codex 格式归一化（双架构一致 + agent/provider 正确）
./build/mini-pilot --agent codex tests/fixtures/raw-codex.jsonl | norm > /tmp/codex-x86.jsonl
qemu-riscv64 ./build/mini-pilot-riscv64 tests/fixtures/raw-codex.jsonl --agent codex | norm > /tmp/codex-rv.jsonl
if diff -q /tmp/codex-x86.jsonl /tmp/codex-rv.jsonl >/dev/null && \
   grep -q '"gen_ai.agent.type": "codex"' /tmp/codex-x86.jsonl && \
   grep -q '"gen_ai.usage.total_tokens": 165' /tmp/codex-x86.jsonl && \
   grep -q '"event.name": "tool.call"' /tmp/codex-x86.jsonl; then
  echo "PASS codex 多 agent 格式归一化（双架构+映射正确）"; pass=$((pass+1))
else echo "FAIL codex 归一化"; fi
echo "结果: $pass/4"
exit $((4-pass))
