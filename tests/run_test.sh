#!/bin/bash
# 双平台一致性测试：x86 与 riscv64(qemu) 输出（剔除不稳定字段后）应一致且符合期望
set -e
cd "$(dirname "$0")/.."
FIX=tests/fixtures/raw-claude.jsonl
EXP=tests/expected/normalized.jsonl
norm(){ sed -E 's/"event.id": "[^"]*"/"event.id": "<ID>"/; s/"observed_time_unix_nano": [0-9]+/"observed_time_unix_nano": <T>"/; s/"host.name": "[^"]*"/"host.name": "<HOST>"/; s/"host.arch": "[^"]*"/"host.arch": "<ARCH>"/'; }

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
# v0.13: 对抗性输入（UTF-8/负数token/坏时间戳/掩码隐含/stdin 管道）
ADV=tests/fixtures/raw-adversarial.jsonl
if ./build/mini-pilot "$ADV" --mask > /tmp/adv-out.jsonl 2>/dev/null && \
   cat "$ADV" | ./build/mini-pilot - > /tmp/adv-stdin.jsonl 2>/dev/null && \
   python3 - <<'PYEOF'
import json
evs = [json.loads(l) for l in open('/tmp/adv-out.jsonl')]
assert len(evs) == 7, f"expect 7 events, got {len(evs)}"
utf8 = evs[0]['gen_ai.input.messages'][0]['content.masked']  # --mask implies content
assert utf8.startswith('fnv1a:')
assert evs[1]['gen_ai.usage.input_tokens'] == 0, "negative tokens must clamp to 0"
assert all(e['time_unix_nano'] > 0 for e in evs), "no zero timestamps (ts fallback)"
assert evs[5]['event.name'] == 'other'  # weird type (line 6)
assert evs[6]['gen_ai.request.model'] == 'm"del'  # quoted model roundtrip
stdin_evs = [json.loads(l) for l in open('/tmp/adv-stdin.jsonl')]
assert len(stdin_evs) == 7, "stdin pipeline (-) must work"
print('ok')
PYEOF
then echo "PASS 对抗性输入（UTF-8/负数钳制/时间戳兜底/掩码隐含/管道）"; pass=$((pass+1))
else echo "FAIL 对抗性输入"; fi
# v0.14: cursor + qoder agent matrix
if ./build/mini-pilot --agent cursor tests/fixtures/raw-cursor.jsonl > /tmp/cur.jsonl 2>/dev/null && \
   ./build/mini-pilot --agent qoder  tests/fixtures/raw-qoder.jsonl  > /tmp/qod.jsonl 2>/dev/null && \
   qemu-riscv64 ./build/mini-pilot-riscv64 --agent cursor tests/fixtures/raw-cursor.jsonl 2>/dev/null | norm > /tmp/cur-rv.jsonl && \
   ./build/mini-pilot --agent cursor tests/fixtures/raw-cursor.jsonl 2>/dev/null | norm > /tmp/cur-x86.jsonl && \
   python3 - <<'PYEOF'
import json
cur = [json.loads(l) for l in open('/tmp/cur.jsonl')]
qod = [json.loads(l) for l in open('/tmp/qod.jsonl')]
assert cur[0]['gen_ai.agent.type'] == 'cursor' and cur[0]['gen_ai.session.id'] == 'cur-77a'
assert cur[1]['gen_ai.usage.total_tokens'] == 228 and cur[2]['event.name'] == 'tool.call'
assert qod[0]['gen_ai.agent.type'] == 'qoder' and qod[0]['gen_ai.provider.name'] == 'qwen'
assert qod[1]['gen_ai.usage.total_tokens'] == 156
print('ok')
PYEOF
then
  diff -q /tmp/cur-x86.jsonl /tmp/cur-rv.jsonl >/dev/null && \
  echo "PASS cursor+qoder agent 矩阵（语义/字段/provider 默认/跨架构）"; pass=$((pass+1))
else echo "FAIL agent 矩阵"; fi
echo "结果: $pass/6"
exit $((6-pass))
