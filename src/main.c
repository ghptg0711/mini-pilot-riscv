// mini-pilot: 将 agent 会话日志规范化为 loongsuite-pilot 的 GenAI 输出事件 schema
// 字段定义来源: loongsuite-pilot/docs/output-event-schema.md
//   Required: time_unix_nano / event.id / event.name / user.id /
//             gen_ai.agent.type / gen_ai.provider.name
//   Recommended: gen_ai.session.id / model / usage.* （按事件类型）
//   Opt-In: gen_ai.input.messages（--content 输出，--mask 时内容脱敏）
// 事件映射: request->llm.request  response->llm.response
//           tool+call->tool.call   tool+result->tool.result
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "jsonlite.h"

#define LINE_MAX 8192
#define FIELD_MAX 512

// ISO8601(UTC) -> unix nanoseconds（受控输入，固定 "YYYY-MM-DDTHH:MM:SSZ"）
static long long iso_to_nano(const char *iso) {
  struct tm tm = {0};
  if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
             &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) return 0;
  tm.tm_year -= 1900; tm.tm_mon -= 1;
  time_t t = timegm(&tm);
  return (long long)t * 1000000000LL;
}

static unsigned long long seq = 0;
// 生成简易全局唯一 event.id（机器前缀+序列+纳秒，教学级实现，说明见 docs）
static void gen_event_id(char *out, int outsz) {
  snprintf(out, outsz, "mp-%llu-%llu", (unsigned long long)time(NULL) % 100000, ++seq);
}

// FNV-1a 64bit —— 用于 --mask 内容脱敏指纹。
// 教学级取舍：非加密哈希，仅用于 demo 展示"内容不可逆输出"机制；
// 生产实现应使用 Pilot masking 文档的规则脱敏或 SHA-256。
static unsigned long long fnv1a(const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; }
  return h;
}

// 采集时刻（observed_time_unix_nano）：与语义时间 time_unix_nano 区分，
// schema 定义"采集器观察到事件的时刻，可能与源事件时间不同"。
static long long now_nano(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// W3C TraceID(32hex)/SpanID(16hex)。确定性派生自 session+序号：
// 优点是测试可复现；生产实现应使用真随机（如 Pilot 依赖的 OTel SDK）。
static void derive_trace(const char *session, char *trace, char *span) {
  unsigned long long a = fnv1a(session), b = fnv1a(session + 1);
  snprintf(trace, 33, "%016llx%016llx", a, b);
  snprintf(span, 17, "%016llx", fnv1a(session) ^ (seq * 0x9e3779b97f4a7c15ULL));
}

// 输出端 JSON 字符串转义（\", \\, 控制字符 -> \uXXXX）。
// v0.9 修复：源文本含引号/反斜杠/换行时，此前会拼出非法 JSON。
static void print_json_str(const char *s) {
  putchar('"');
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    switch (c) {
      case '"':  fputs("\\\"", stdout); break;
      case '\\': fputs("\\\\", stdout); break;
      case '\n': fputs("\\n", stdout); break;
      case '\r': fputs("\\r", stdout); break;
      case '\t': fputs("\\t", stdout); break;
      default:
        if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
  }
  putchar('"');
}

static const char *map_event_name(const char *type, const char *status) {
  if (strcmp(type, "request") == 0)  return "llm.request";
  if (strcmp(type, "response") == 0) return "llm.response";
  if (strcmp(type, "tool") == 0)
    return (strcmp(status, "result") == 0) ? "tool.result" : "tool.call";
  return "other";
}

// ---- 多 agent 格式适配层（v0.8）--------------------------------------
// Pilot 的核心价值：不同 agent 的原生日志字段各异，采集器归一化后输出统一
// GenAI 事件。这里用"字段别名表"演示同一机制的最小实现。
typedef struct {
  char ts[FIELD_MAX], type[64], status[64], model[FIELD_MAX];
  char session[FIELD_MAX], tool[FIELD_MAX], text[LINE_MAX];
  double tin, tout;
} record_t;

// claude-code 格式: ts/type/status/model/session/tool/text/tokens_in/tokens_out
// codex      格式: timestamp/event_type/model/conversation_id/input_tokens/output_tokens
//                     event_type: model_request|model_response|tool_use
static void read_record(const char *line, record_t *r) {
  memset(r, 0, sizeof(*r));
  json_get_str(line, "ts",        r->ts,      sizeof(r->ts));
  json_get_str(line, "timestamp", r->ts,      sizeof(r->ts));      // 别名（codex）
  json_get_str(line, "type",      r->type,    sizeof(r->type));
  json_get_str(line, "event_type", r->type,   sizeof(r->type));    // 别名（codex）
  json_get_str(line, "status",    r->status,  sizeof(r->status));
  json_get_str(line, "model",     r->model,   sizeof(r->model));
  json_get_str(line, "session",   r->session, sizeof(r->session));
  json_get_str(line, "conversation_id", r->session, sizeof(r->session)); // 别名
  json_get_str(line, "tool",      r->tool,    sizeof(r->tool));
  json_get_str(line, "text",      r->text,    sizeof(r->text));
  json_get_str(line, "prompt",    r->text,    sizeof(r->text));    // 别名（codex 请求）
  json_get_str(line, "reply",     r->text,    sizeof(r->text));    // 别名（codex 响应）
  json_get_num(line, "tokens_in",   &r->tin);
  json_get_num(line, "input_tokens", &r->tin);                     // 别名（codex）
  json_get_num(line, "tokens_out",  &r->tout);
  json_get_num(line, "output_tokens", &r->tout);                   // 别名（codex）
  // codex 的事件语义归一
  if (strcmp(r->type, "model_request") == 0)  strcpy(r->type, "request");
  if (strcmp(r->type, "model_response") == 0) strcpy(r->type, "response");
  if (strcmp(r->type, "tool_use") == 0)       strcpy(r->type, "tool");
}

// 命令行: mini-pilot <session.jsonl> [--agent claude-code|codex] [--content] [--mask]
//   --agent    源 agent 类型（决定 gen_ai.agent.type / gen_ai.provider.name，
//              默认 claude-code；codex 走别名表解析其原生日志格式）
//   --content  输出 Opt-In 内容字段 gen_ai.input.messages（默认不输出，遵循 schema 的 Opt-In 语义）
//   --mask     配合 --content，将内容替换为 FNV-1a 指纹（脱敏演示）
int main(int argc, char **argv) {
  int emit_content = 0, mask = 0, is_codex = 0;
  const char *path = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--content") == 0) emit_content = 1;
    else if (strcmp(argv[i], "--mask") == 0) mask = 1;
    else if (strcmp(argv[i], "--agent") == 0 && i + 1 < argc) {
      is_codex = (strcmp(argv[++i], "codex") == 0);
    }
    else if (!path) path = argv[i];
  }
  if (!path) {
    fprintf(stderr, "usage: %s <session.jsonl> [--agent claude-code|codex] [--content] [--mask]\n", argv[0]);
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { perror("fopen"); return 1; }

  char line[LINE_MAX];
  char hostname[64] = "unknown";
  gethostname(hostname, sizeof(hostname) - 1);   // 真实主机名（v0.9 去除硬编码）
  while (fgets(line, sizeof(line), f)) {
    // v0.9 修复：行长度超过缓冲时 fgets 会截断——检测并丢弃该坏行
    size_t len = strlen(line);
    if (len == sizeof(line) - 1 && line[len - 1] != '\n') {
      fprintf(stderr, "warn: line %llu exceeds %d bytes, skipped\n",
              (unsigned long long)(seq + 1), (int)sizeof(line));
      int ch; while ((ch = fgetc(f)) != '\n' && ch != EOF) {}
      continue;
    }
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0') continue;

    record_t r;
    read_record(line, &r);   // 多 agent 字段别名归一

    char eid[64]; gen_event_id(eid, sizeof(eid));
    const char *ename = map_event_name(r.type, r.status);
    long long nano = iso_to_nano(r.ts);

    // ---- 输出 GenAI 事件（Required/Recommended 字段子集）----
    char trace[33] = "", span[17] = "";
    if (r.session[0]) derive_trace(r.session, trace, span);
    long long onano = now_nano();
    printf("{\"time_unix_nano\": %lld, ", nano);
    printf("\"observed_time_unix_nano\": %lld, ", onano);
    printf("\"event.id\": \"%s\", ", eid);
    printf("\"event.name\": \"%s\", ", ename);
    printf("\"user.id\": \"local-user\", ");
    if (trace[0]) printf("\"trace_id\": \"%s\", \"span_id\": \"%s\", ", trace, span);
    printf("\"host.name\": \"%s\", ", hostname);
    printf("\"gen_ai.agent.type\": \"%s\", ", is_codex ? "codex" : "claude-code");
    printf("\"gen_ai.provider.name\": \"%s\"", is_codex ? "openai" : "anthropic");
    if (r.session[0]) { printf(", \"gen_ai.session.id\": "); print_json_str(r.session); }
    if (r.model[0]) {
      if (strcmp(ename, "llm.request") == 0)
        { printf(", \"gen_ai.request.model\": "); print_json_str(r.model); }
      else
        { printf(", \"gen_ai.response.model\": "); print_json_str(r.model); }
    }
    if (strcmp(ename, "tool.call") == 0 && r.tool[0])
      { printf(", \"tool.name\": "); print_json_str(r.tool); }
    if (strcmp(ename, "llm.response") == 0) {
      printf(", \"gen_ai.usage.input_tokens\": %d, \"gen_ai.usage.output_tokens\": %d, \"gen_ai.usage.total_tokens\": %d",
             (int)r.tin, (int)r.tout, (int)(r.tin + r.tout));
    }
    // Opt-In 内容字段（--content）：llm.request 的用户输入 / llm.response 的输出文本
    if (emit_content && r.text[0] &&
        (strcmp(ename, "llm.request") == 0 || strcmp(ename, "llm.response") == 0)) {
      const char *role = (strcmp(ename, "llm.request") == 0) ? "user" : "assistant";
      if (mask)
        printf(", \"gen_ai.input.messages\": [{\"role\": \"%s\", \"content.masked\": \"fnv1a:%016llx\"}]",
               role, fnv1a(r.text));
      else {
        printf(", \"gen_ai.input.messages\": [{\"role\": \"%s\", \"content\": ", role);
        print_json_str(r.text);
        printf("}]");
      }
    }
    printf("}\n");
  }
  fclose(f);
  return 0;
}
