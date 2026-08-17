# loongsuite-pilot 学习笔记（mini-pilot-riscv 的设计依据）

> 仓库：https://github.com/alibaba/loongsuite-pilot（v1.2.0，2026-08-17 本地克隆研究）
> 本笔记服务于 OSPP 266eb0010《LoongSuite Pilot 支持 RISC-V 架构》申请，也是 demo 的设计依据。

## 1. Pilot 是什么

本地 AI 编程智能体遥测采集器：发现开发者机器上的 agent（Claude Code/Codex/Cursor 等）→
安装 hook/插件 → 读取本地日志 → 规范化为统一 GenAI 事件 → 导出到 JSONL/SLS/HTTP/OTLP。

## 2. 技术栈与"RISC-V 适配"的真实含义

- **TypeScript / Node.js** 项目（package.json + tsconfig + vitest；`build.mjs` 构建）
- 关键依赖：`sqlite3`（**原生模块**）、`express`、`pino`（日志）、OpenTelemetry 系列（OTLP 导出）
- 因此"支持 RISC-V 架构"不是重写，而是：
  1. **Node.js 运行时在 riscv64 可用**（官方无 riscv64 发布版；openEuler 社区/源码自编是两条路）
  2. **sqlite3 原生模块 riscv64 交叉编译**（node-gyp + riscv64 工具链，或评估换纯 JS 方案）
  3. 平台相关代码路径核查（agent 发现路径、hook 安装位置在 Linux 语义下通用）
  4. **CI 增加 riscv64 验证**（qemu-user 跑单测 / 社区 riscv64 runner）
- demo 中我用 C 复刻的是第 2-3 层的"采集→规范化→交叉编译→qemu 验证"闭环，验证申请者具备
  架构移植的方法论（受控输入、双平台一致性测试、静态链接）。

## 3. GenAI 事件 schema 要点（demo 实现的子集）

| 字段 | 级别 | demo 实现 |
|---|---|---|
| time_unix_nano / event.id / event.name / user.id | Required | ✓ |
| gen_ai.agent.type / gen_ai.provider.name | Required | ✓（claude-code/anthropic） |
| gen_ai.session.id | Cond. Required | ✓ |
| gen_ai.request/response.model | Cond. Required | ✓ |
| gen_ai.usage.input/output/total_tokens | Recommended | ✓ |
| trace_id/span_id/observed_time/host.name/service.name | Recommended | host.name ✓，其余未实现 |
| gen_ai.input.messages 等 | Opt-In（敏感） | 未实现（对应 masking 机制） |

事件名映射：`request→llm.request`，`response→llm.response`，`tool+call/result→tool.call/tool.result`。

## 4. 工程组织上学到的

- `tests/fixtures` + `tests/expected` + 契约测试（`tests/contract/agent-activity-schema.ts`）的
  "样例驱动 schema 测试"思路 → demo 的 `run_test.sh` 三段式（期望一致/跨架构一致/schema 断言）即由此而来
- 中英双语文档同步（docs/ 与 docs/zh-CN/）→ 贡献 PR 时文档要双语
- `AGENTS.md`/`SKILL.md`/`CLAUDE.md`：面向 AI 协作的仓库说明——新兴工程实践，值得借鉴进自己的仓库

## 5. 后续迭代方向（若申请立项）

1. v0.6：`--mask` 脱敏实现（对应 docs/masking.md，text 字段哈希化）✅ 已完成
2. v0.7：`observed_time_unix_nano` 与 trace/span 生成 ✅ 已完成
3. v0.8：Codex 格式 fixture（多 agent 归一化）✅ 已完成

## 6. 工程风格对齐（v0.12，2026-08-17）

以上游仓库为范本做了一次风格靠拢：

| 上游惯例 | 本仓库对齐 |
|---|---|
| conventional commits（`fix(dsh): ... (#PR)`） | v0.12 起采用 `refactor(src):`/`docs(readme):`/`chore:` 三段式 |
| src/ 组件分层（inputs/flushers/mask/core/cli） | 拆分为 input/flusher/mask/cli 四组件+main 编排，每个模块头注释标注上游对应物 |
| README.md（英）+ README.zh-CN.md（中） | 同名结构互换对齐，双向互链 |
| AGENTS.md（AI 协作规范） | 新增，含 code map（每组件标注上游类比）、约定、"不得静默修复"的教学级取舍清单 |
| 代码注释英文 | 全量切换为英文注释 |
| CI：name: CI + 步骤化（typecheck/test/build） | 已有 ci.yml 覆盖 build/test/smoke（C 项目无 typecheck 等价物） |

对求职的启示：给一个仓库提 PR 前，先读它的提交历史/目录组织/注释语言并模仿——
这比代码本身更快获得 maintainer 好感（PLCT 的 issue 规范描述要求同理）。
