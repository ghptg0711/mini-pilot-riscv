# mini-pilot-riscv —— 学习 loongsuite-pilot 的迷你遥测采集器
# 目标: x86 与 riscv64 双平台构建 + qemu-riscv64 验证
CC_x86    = gcc
CC_riscv  = riscv64-linux-gnu-gcc
CFLAGS    = -Wall -Wextra -O2 -static
SRC       = src/main.c src/cli.c src/input.c src/flusher.c src/mask.c src/jsonlite.c
QEMU      = qemu-riscv64   # 来自 /home/gh/env/qemu（PATH 中）

.PHONY: all x86 riscv run-x86 run-riscv test bench clean

all: x86 riscv

x86:
	mkdir -p build
	$(CC_x86) $(CFLAGS) -o build/mini-pilot $(SRC) -Isrc

riscv:
	mkdir -p build
	$(CC_riscv) $(CFLAGS) -o build/mini-pilot-riscv64 $(SRC) -Isrc
	@file build/mini-pilot-riscv64 | grep -q RISC-V || (echo "交叉编译产物非 RISC-V"; exit 1)

run-x86: x86
	./build/mini-pilot tests/fixtures/raw-claude.jsonl

run-riscv: riscv
	$(QEMU) ./build/mini-pilot-riscv64 tests/fixtures/raw-claude.jsonl

test: all
	./tests/run_test.sh

bench: all
	bash scripts/bench.sh

clean:
	rm -rf build
