#!/bin/bash
# 端到端冒烟脚本——对应目标项目"产出要求 3"的可复现验证形态：
#   安装检查 → 构建(x86+riscv64) → 启动(status) → 采集/输出(双架构) → 输出校验
# 与 run_test.sh 的区别：本脚本按"安装→启动→采集"的运维链路组织，
# run_test.sh 专注于输出正确性断言。
set -e
cd "$(dirname "$0")/.."

echo "== [1/5] 安装检查 =="
check(){ command -v "$1" >/dev/null 2>&1 && echo "  ok   $1 ($($1 --version 2>&1 | head -1 | cut -c1-40))" || { echo "  miss $1"; MISS=1; }; }
MISS=0; check gcc; check riscv64-linux-gnu-gcc; check qemu-riscv64; [ $MISS -eq 0 ] || { echo "依赖缺失，中止"; exit 1; }

echo "== [2/5] 构建（x86 + riscv64） =="
make all >/dev/null

echo "== [3/5] 启动（运维命令 status，双架构） =="
./build/mini-pilot status | sed 's/^/  x86   /'
qemu-riscv64 ./build/mini-pilot-riscv64 status | sed 's/^/  riscv /'

echo "== [4/5] 采集/输出（双架构各一条链路） =="
./build/mini-pilot tests/fixtures/raw-claude.jsonl            > /tmp/smoke-x86.jsonl
qemu-riscv64 ./build/mini-pilot-riscv64 tests/fixtures/raw-claude.jsonl > /tmp/smoke-rv.jsonl
echo "  x86   事件数: $(wc -l < /tmp/smoke-x86.jsonl)"
echo "  riscv 事件数: $(wc -l < /tmp/smoke-rv.jsonl)"

echo "== [5/5] 输出校验 =="
python3 - <<'EOF'
import json
for name in ("/tmp/smoke-x86.jsonl", "/tmp/smoke-rv.jsonl"):
    evs = [json.loads(l) for l in open(name)]
    assert evs and all(e["event.name"] for e in evs)
    arch = {e["host.arch"] for e in evs}
    print(f"  ok   {name}: {len(evs)} 事件, host.arch={arch}")
EOF
echo "冒烟通过：安装→启动→采集→输出 链路可复现"
