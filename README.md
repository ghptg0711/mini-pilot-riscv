# mini-pilot-riscv

> 一个用于 OSPP 2026 点亮计划申请（项目 266eb0010《LoongSuite Pilot 支持 RISC-V 架构》）的能力验证 demo：
> 用 C 实现的**迷你 AI 编程智能体遥测采集器**，将 agent 会话日志规范化为
> [loongsuite-pilot](https://github.com/alibaba/loongsuite-pilot) 的 GenAI 事件 schema，
> 支持 **x86 与 riscv64 交叉编译**，并可在 `qemu-riscv64` 用户模式直接运行验证。

## 它验证了什么（对应目标项目的技术环节）

| 目标项目环节 | 本 demo 的对应实现 |
|---|---|
| 理解 GenAI 输出事件模型 | `src/main.c` 按 `docs/output-event-schema.md` 映射 Required/Recommended 字段 |
| 日志采集与规范化 | JSONL 逐行解析 → `llm.request / llm.response / tool.call / tool.result` |
| RISC-V 架构适配 | `make riscv` 交叉编译（`riscv64-linux-gnu-gcc -static`） |
| 无真机条件下的验证 | `make run-riscv` 在 qemu user 模式跑通全流程 |
| 测试与工程化 | `make test`：期望输出比对 + 双平台一致性 + schema 断言 |

## 快速开始

```bash
make all          # 构建 x86 + riscv64
make run-x86      # 本机运行
make run-riscv    # qemu-riscv64 运行（需 qemu-riscv64 在 PATH）
make test         # 3 项测试
```

## 输入 / 输出示例

输入（agent 会话日志，`tests/fixtures/raw-claude.jsonl`）：
```json
{"ts": "2026-08-17T10:00:01Z", "type": "request", "model": "claude-sonnet-4", "text": "help me sort an array", "session": "s-a1b2"}
```

输出（GenAI 规范化事件）：
```json
{"time_unix_nano": 1786960801000000000, "event.id": "mp-xxxx-1", "event.name": "llm.request", "user.id": "local-user", "host.name": "riscv-qemu", "gen_ai.agent.type": "claude-code", "gen_ai.provider.name": "anthropic", "gen_ai.session.id": "s-a1b2", "gen_ai.request.model": "claude-sonnet-4"}
```

## 设计说明与已知局限

- `src/jsonlite.[ch]`：零依赖 JSON 行解析器，仅支持**扁平对象 + 字符串/数字/布尔**的受控输入
  （真实项目应使用成熟解析库；零依赖是为了 riscv64 静态交叉编译的教学取舍，已在头文件注释中声明）。
- `event.id` 为教学级实现（时间戳+序列），生产实现应使用 UUID v4（目标项目依赖 `uuid` 库）。
- 输入为模拟的 Claude Code 会话格式；真实 Pilot 的 agent discovery / hook 注入等环节不在本 demo 范围。

## 学习来源（不凭空创造）

- 事件模型、字段定义、命名映射：`alibaba/loongsuite-pilot` 的 `docs/output-event-schema.md`
- 工程组织（fixtures/expected/一致性测试）：参考该仓库 `tests/` 目录组织方式
- 详细笔记见 [docs/study-notes.md](docs/study-notes.md)

## License

MIT（与学习对象仓库一致）
