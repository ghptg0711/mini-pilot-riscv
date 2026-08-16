// v0.2: 规范化为 loongsuite-pilot 的 GenAI 输出事件 schema（核心字段子集）
// 字段定义来源: loongsuite-pilot/docs/output-event-schema.md
//   time_unix_nano(必) / event.id(必) / event.name(必) / user.id(必)
//   gen_ai.agent.type(必) / gen_ai.provider.name(必)
//   gen_ai.session.id / gen_ai.request|response.model / gen_ai.usage.*
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

static const char *map_event_name(const char *type, const char *status) {
  if (strcmp(type, "request") == 0)  return "llm.request";
  if (strcmp(type, "response") == 0) return "llm.response";
  if (strcmp(type, "tool") == 0)
    return (strcmp(status, "result") == 0) ? "tool.result" : "tool.call";
  return "other";
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: %s <session.jsonl> [--mask]\n", argv[0]); return 1; }
  FILE *f = fopen(argv[1], "r");
  if (!f) { perror("fopen"); return 1; }

  char line[LINE_MAX];
  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0') continue;

    char ts[FIELD_MAX] = "", type[64] = "", status[64] = "", model[FIELD_MAX] = "";
    char session[FIELD_MAX] = "", tool[FIELD_MAX] = "";
    double tin = 0, tout = 0;
    json_get_str(line, "ts", ts, sizeof(ts));
    json_get_str(line, "type", type, sizeof(type));
    json_get_str(line, "status", status, sizeof(status));
    json_get_str(line, "model", model, sizeof(model));
    json_get_str(line, "session", session, sizeof(session));
    json_get_str(line, "tool", tool, sizeof(tool));
    json_get_num(line, "tokens_in", &tin);
    json_get_num(line, "tokens_out", &tout);

    char eid[64]; gen_event_id(eid, sizeof(eid));
    const char *ename = map_event_name(type, status);
    long long nano = iso_to_nano(ts);

    // ---- 输出 GenAI 事件（Required/Recommended 字段子集）----
    printf("{\"time_unix_nano\": %lld, ", nano);
    printf("\"event.id\": \"%s\", ", eid);
    printf("\"event.name\": \"%s\", ", ename);
    printf("\"user.id\": \"local-user\", ");
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
    printf("}\n");
  }
  fclose(f);
  return 0;
}
