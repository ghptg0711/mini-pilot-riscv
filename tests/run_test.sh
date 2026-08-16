#!/bin/bash
# 双平台一致性测试：x86 与 riscv64(qemu) 输出（剔除不稳定字段后）应一致且符合期望
set -e
cd "$(dirname "$0")/.."
FIX=tests/fixtures/raw-claude.jsonl
EXP=tests/expected/normalized.jsonl
norm(){ sed -E 's/"event.id": "[^"]*"/"event.id": "<ID>"/; s/"observed_time_unix_nano": [0-9]+/"observed_time_unix_nano": <T>"/'; }

./build/mini-pilot            "$FIX" | norm > /tmp/out-x86.jsonl
qemu-riscv64 ./build/mini-pilot-riscv64 "$FIX" | norm > /tmp/out-riscv.jsonl

pass=0
diff /tmp/out-x86.jsonl   "$EXP" && { echo "PASS x86   输出与期望一致"; pass=$((pass+1)); }
diff /tmp/out-x86.jsonl /tmp/out-riscv.jsonl && { echo "PASS 跨架构 x86==riscv64 输出一致"; pass=$((pass+1)); }
python3 -c "import json,sys
for l in open('$EXP'):
    e=json.loads(l)
    assert e['time_unix_nano']>0 and e['event.name'] and e['user.id'] and e['gen_ai.agent.type']
print('PASS 期望文件自身满足 schema Required 字段')"; pass=$((pass+1))
echo "结果: $pass/3"
exit $((3-pass))
