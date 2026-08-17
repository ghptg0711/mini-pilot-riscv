# mini-pilot-riscv

[![ci](https://github.com/ghptg0711/mini-pilot-riscv/actions/workflows/ci.yml/badge.svg)](https://github.com/ghptg0711/mini-pilot-riscv/actions/workflows/ci.yml)

> 用 C 实现的**迷你 AI 编程智能体遥测采集器**——OSPP 2026 点亮计划项目
> [266eb0010《LoongSuite Pilot 支持 RISC-V 架构》](https://summer.ospp.ac.cn/org/prodetail/266eb0010?lang=zh&list=pro)
> 的申请能力验证 demo（English: [README.en.md](README.en.md)）。
> 它把 agent 会话日志规范化为 [loongsuite-pilot](https://github.com/alibaba/loongsuite-pilot)
> 的 GenAI 事件 schema，支持 **x86 与 riscv64 交叉编译**，并在 `qemu-riscv64` 用户模式直接运行验证。
> **CI 在每次 push 时自动完成双架构构建、跨架构一致性测试与 riscv64 冒烟运行。**

## 功能特性

| 特性 | 说明 | 版本 |
|---|---|---|
| GenAI 事件规范化 | 按 Pilot `docs/output-event-schema.md` 输出 Required/Recommended 字段 | v0.2 |
| riscv64 交叉编译 | `riscv64-linux-gnu-gcc -static`，qemu user 模式免 rootfs 运行 | v0.3 |
| 三段式测试 | 期望比对 / 双架构逐字节一致 / schema 断言 | v0.4 |
| 内容脱敏 | `--content` 输出 Opt-In 内容，`--mask` 以 FNV-1a 指纹替代明文 | v0.6 |
| 采集时间与追踪 | `observed_time_unix_nano` + W3C `trace_id`/`span_id`（同会话同 trace） | v0.7 |
| 多 agent 归一化 | `--agent codex` 以字段别名表解析 Codex 原生日志 | v0.8 |
| 运维命令与架构感知 | `mini-pilot status`（版本/构建架构/运行架构）、事件含 `host.arch`、`scripts/smoke.sh` 端到端冒烟 | v0.11 |

> v0.9-v0.11 的转义安全、CI、status/冒烟链路分别对应目标项目"产出要求"中的
> 可靠性、可持续验证（CI）与"安装→启动→采集/输出"可复现验收项。

## 快速开始

```bash
make all                                    # 构建 x86 + riscv64
./build/mini-pilot status                   # 运维状态命令（双架构可运行）
make run-x86                                # 本机运行（claude-code 格式）
make run-riscv                              # qemu-riscv64 运行
./build/mini-pilot --agent codex tests/fixtures/raw-codex.jsonl   # codex 格式
./build/mini-pilot tests/fixtures/raw-claude.jsonl --content --mask  # 脱敏输出
make test                                   # 4 项测试
bash scripts/smoke.sh                       # 端到端冒烟（安装检查→构建→启动→采集→校验）
```

依赖：`gcc`、`riscv64-linux-gnu-gcc`、`qemu-riscv64`、`python3`（测试断言用）。
参考环境：[/home/gh/env](../env) 提供全套工具链的安装清单与验证脚本。

## 数据流（文字版架构图）

```
agent 会话日志(JSONL, 各家格式不同)          统一 GenAI 事件(JSONL)
┌──────────────────────┐   ┌──────────────────┐   ┌─────────────────────────┐
│ raw-claude.jsonl     │   │ 字段别名表+语义归一 │   │ time_unix_nano          │
│  ts/type/text/...    │──▶│ read_record()     │──▶│ event.name/agent.type   │
│ raw-codex.jsonl      │   │ map_event_name()  │   │ usage.*/trace_id/span_id│
│  timestamp/prompt/.. │   │ (--mask 脱敏)     │   │ (Opt-In 内容/脱敏指纹)   │
└──────────────────────┘   └──────────────────┘   └─────────────────────────┘
        x86 gcc / riscv64 交叉编译(-static) →  本机 / qemu-riscv64 运行
```

## 输入 / 输出示例

输入（`tests/fixtures/raw-claude.jsonl`）：
```json
{"ts": "2026-08-17T10:00:01Z", "type": "request", "model": "claude-sonnet-4", "text": "help me sort an array", "session": "s-a1b2"}
```

输出：
```json
{"time_unix_nano": 1786960801000000000, "observed_time_unix_nano": ..., "event.id": "mp-xxxx-1", "event.name": "llm.request", "user.id": "local-user", "trace_id": "548f...", "span_id": "cab8...", "host.name": "riscv-qemu", "gen_ai.agent.type": "claude-code", "gen_ai.provider.name": "anthropic", "gen_ai.session.id": "s-a1b2", "gen_ai.request.model": "claude-sonnet-4"}
```

## 与目标项目（266eb0010）的对应关系

| 目标项目环节 | 本 demo 的对应实现 |
|---|---|
| 理解 GenAI 输出事件模型 | `src/main.c` 的字段映射（Required/Recommended/Opt-In 分层） |
| 日志采集与规范化 | JSONL 逐行 → `llm.request/llm.response/tool.call/tool.result` |
| RISC-V 架构适配 | `make riscv` 交叉编译 + `make run-riscv` QEMU 验证 |
| 无真机条件验证 | qemu user 模式全流程；双架构输出逐字节一致断言 |
| 测试与工程化 | fixtures/expected/契约断言（学自 Pilot 的 tests/ 组织） |

## 设计取舍与已知局限（诚实声明）

- `src/jsonlite.[ch]`：零依赖 JSON 解析器，仅支持**扁平对象 + 字符串/数字/布尔**受控输入，
  不处理转义/嵌套——真实项目应用成熟解析库（Pilot 用 jsonc-parser）；零依赖是为 riscv64
  静态交叉编译的教学取舍，头文件注释已声明。
- `event.id`/`trace_id`：确定性派生（时间+序列 / session 哈希），换取测试可复现；
  生产实现应使用 UUID 与 OTel SDK 真随机。
- 脱敏用非加密 FNV-1a：仅演示"内容不可逆输出"机制；生产应按 Pilot `docs/masking.md`
  规则脱敏或 SHA-256。
- agent discovery / hook 注入等 Pilot 环节不在本 demo 范围。

## 学习来源（不凭空创造）

- 事件模型与字段定义：`alibaba/loongsuite-pilot` 的 `docs/output-event-schema.md`
- 测试组织（fixtures/expected/契约测试）：该仓库 `tests/` 目录
- 详细研究笔记：[docs/study-notes.md](docs/study-notes.md)

## License

Apache-2.0（与学习对象仓库 loongsuite-pilot 一致）
