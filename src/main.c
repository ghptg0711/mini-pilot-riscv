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

static const char *map_event_name(const char *type, const char *status) {
  if (strcmp(type, "request") == 0)  return "llm.request";
  if (strcmp(type, "response") == 0) return "llm.response";
  if (strcmp(type, "tool") == 0)
    return (strcmp(status, "result") == 0) ? "tool.result" : "tool.call";
  return "other";
}

// 命令行: mini-pilot <session.jsonl> [--content] [--mask]
//   --content  输出 Opt-In 内容字段 gen_ai.input.messages（默认不输出，遵循 schema 的 Opt-In 语义）
//   --mask     配合 --content，将内容替换为 FNV-1a 指纹（脱敏演示）
int main(int argc, char **argv) {
  int emit_content = 0, mask = 0;
  const char *path = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--content") == 0) emit_content = 1;
    else if (strcmp(argv[i], "--mask") == 0) mask = 1;
    else if (!path) path = argv[i];
  }
  if (!path) {
    fprintf(stderr, "usage: %s <session.jsonl> [--content] [--mask]\n", argv[0]);
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { perror("fopen"); return 1; }

  char line[LINE_MAX];
  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0') continue;

    char ts[FIELD_MAX] = "", type[64] = "", status[64] = "", model[FIELD_MAX] = "";
    char session[FIELD_MAX] = "", tool[FIELD_MAX] = "", text[LINE_MAX] = "";
    double tin = 0, tout = 0;
    json_get_str(line, "ts", ts, sizeof(ts));
    json_get_str(line, "type", type, sizeof(type));
    json_get_str(line, "status", status, sizeof(status));
    json_get_str(line, "model", model, sizeof(model));
    json_get_str(line, "session", session, sizeof(session));
    json_get_str(line, "tool", tool, sizeof(tool));
    json_get_str(line, "text", text, sizeof(text));
    json_get_num(line, "tokens_in", &tin);
    json_get_num(line, "tokens_out", &tout);

    char eid[64]; gen_event_id(eid, sizeof(eid));
    const char *ename = map_event_name(type, status);
    long long nano = iso_to_nano(ts);

    // ---- 输出 GenAI 事件（Required/Recommended 字段子集）----
    char trace[33] = "", span[17] = "";
    if (session[0]) derive_trace(session, trace, span);
    long long onano = now_nano();
    printf("{\"time_unix_nano\": %lld, ", nano);
    printf("\"observed_time_unix_nano\": %lld, ", onano);
    printf("\"event.id\": \"%s\", ", eid);
    printf("\"event.name\": \"%s\", ", ename);
    printf("\"user.id\": \"local-user\", ");
    if (trace[0]) printf("\"trace_id\": \"%s\", \"span_id\": \"%s\", ", trace, span);
    printf("\"host.name\": \"%s\", ", strcmp(ename, "") == 0 ? "" : "riscv-qemu");
    printf("\"gen_ai.agent.type\": \"claude-code\", ");
    printf("\"gen_ai.provider.name\": \"anthropic\"");
    if (session[0]) printf(", \"gen_ai.session.id\": \"%s\"", session);
    if (model[0]) {
      if (strcmp(ename, "llm.request") == 0)
        printf(", \"gen_ai.request.model\": \"%s\"", model);
      else
        printf(", \"gen_ai.response.model\": \"%s\"", model);
    }
    if (strcmp(ename, "tool.call") == 0 && tool[0])
      printf(", \"tool.name\": \"%s\"", tool);
    if (strcmp(ename, "llm.response") == 0) {
      printf(", \"gen_ai.usage.input_tokens\": %d, \"gen_ai.usage.output_tokens\": %d, \"gen_ai.usage.total_tokens\": %d",
             (int)tin, (int)tout, (int)(tin + tout));
    }
    // Opt-In 内容字段（--content）：llm.request 的用户输入 / llm.response 的输出文本
    if (emit_content && text[0] &&
        (strcmp(ename, "llm.request") == 0 || strcmp(ename, "llm.response") == 0)) {
      const char *role = (strcmp(ename, "llm.request") == 0) ? "user" : "assistant";
      if (mask)
        printf(", \"gen_ai.input.messages\": [{\"role\": \"%s\", \"content.masked\": \"fnv1a:%016llx\"}]",
               role, fnv1a(text));
      else
        printf(", \"gen_ai.input.messages\": [{\"role\": \"%s\", \"content\": \"%s\"}]", role, text);
    }
    printf("}\n");
  }
  fclose(f);
  return 0;
}
