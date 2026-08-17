#include "flusher.h"
#include "common.h"
#include "mask.h"
#include "jsonlite.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/utsname.h>

/* ---- normalization helpers (used only at emission time) ---- */

/* ISO8601(UTC) "YYYY-MM-DDTHH:MM:SSZ" -> unix nanoseconds (controlled input). */
static long long iso_to_nano(const char *iso) {
  struct tm tm = {0};
  if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
             &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6)
    return 0;
  tm.tm_year -= 1900;
  tm.tm_mon -= 1;
  time_t t = timegm(&tm);
  return (long long)t * 1000000000LL;
}

/* Collection time; schema: "may differ from the source event time". */
static long long now_nano(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static unsigned long long seq = 0;

/* Deterministic event.id (time+seq): reproducible tests; production
 * should use a UUID library (upstream depends on `uuid`). */
static void gen_event_id(char *out, int outsz) {
  snprintf(out, outsz, "mp-%llu-%llu",
           (unsigned long long)time(NULL) % 100000, ++seq);
}

/* W3C trace/span derived deterministically from the session id:
 * same session -> same trace_id, per-event span_id. Production should
 * use real randomness (upstream uses the OTel SDK). */
static void derive_trace(const char *session, char *trace, char *span) {
  unsigned long long a = mask_fnv1a(session), b = mask_fnv1a(session + 1);
  snprintf(trace, 33, "%016llx%016llx", a, b);
  snprintf(span, 17, "%016llx",
           mask_fnv1a(session) ^ (seq * 0x9e3779b97f4a7c15ULL));
}

static const char *map_event_name(const char *type, const char *status) {
  if (strcmp(type, "request") == 0)  return "llm.request";
  if (strcmp(type, "response") == 0) return "llm.response";
  if (strcmp(type, "tool") == 0)
    return (strcmp(status, "result") == 0) ? "tool.result" : "tool.call";
  return "other";
}

/* Compile-time arch detection, mirroring upstream installer's
 * uname -m / process.arch probing. */
static const char *build_arch(void) {
#if defined(__riscv) && __riscv_xlen == 64
  return "riscv64";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__aarch64__)
  return "aarch64";
#else
  return "unknown";
#endif
}

/* Output-side JSON string escaping (\", \\, control chars). */
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

void flusher_emit(const record_t *r, const flusher_opts_t *opts) {
  char eid[64];
  gen_event_id(eid, sizeof(eid));
  const char *ename = map_event_name(r->type, r->status);
  long long onano = now_nano();
  /* Semantic event time is Required by the schema; when the source
   * timestamp is unparsable, fall back to the collection time rather
   * than emitting the invalid value 0 (best-effort collector). */
  long long nano = iso_to_nano(r->ts);
  if (nano <= 0) nano = onano;

  /* Token counts are semantically non-negative; clamp garbage input. */
  double tin = r->tin > 0 ? r->tin : 0;
  double tout = r->tout > 0 ? r->tout : 0;

  char trace[33] = "", span[17] = "";
  if (r->session[0]) derive_trace(r->session, trace, span);
  char hostname[64] = "unknown";
  gethostname(hostname, sizeof(hostname) - 1);

  /* Required / Recommended field subset per docs/output-event-schema.md */
  printf("{\"time_unix_nano\": %lld, ", nano);
  printf("\"observed_time_unix_nano\": %lld, ", onano);
  printf("\"event.id\": \"%s\", ", eid);
  printf("\"event.name\": \"%s\", ", ename);
  printf("\"user.id\": \"local-user\", ");
  if (trace[0]) printf("\"trace_id\": \"%s\", \"span_id\": \"%s\", ", trace, span);
  printf("\"host.name\": \"%s\", ", hostname);
  printf("\"host.arch\": \"%s\", ", build_arch());
  printf("\"gen_ai.agent.type\": \"%s\", ", opts->is_codex ? "codex" : "claude-code");
  printf("\"gen_ai.provider.name\": \"%s\"", opts->is_codex ? "openai" : "anthropic");
  if (r->session[0]) { printf(", \"gen_ai.session.id\": "); print_json_str(r->session); }
  if (r->model[0]) {
    if (strcmp(ename, "llm.request") == 0) { printf(", \"gen_ai.request.model\": "); print_json_str(r->model); }
    else { printf(", \"gen_ai.response.model\": "); print_json_str(r->model); }
  }
  if (strcmp(ename, "tool.call") == 0 && r->tool[0])
    { printf(", \"tool.name\": "); print_json_str(r->tool); }
  if (strcmp(ename, "llm.response") == 0) {
    printf(", \"gen_ai.usage.input_tokens\": %d, \"gen_ai.usage.output_tokens\": %d, \"gen_ai.usage.total_tokens\": %d",
           (int)tin, (int)tout, (int)(tin + tout));
  }
  /* Opt-In content (see docs/output-event-schema.md "Opt-In" level). */
  if (opts->emit_content && r->text[0] &&
      (strcmp(ename, "llm.request") == 0 || strcmp(ename, "llm.response") == 0)) {
    const char *role = (strcmp(ename, "llm.request") == 0) ? "user" : "assistant";
    if (opts->mask)
      printf(", \"gen_ai.input.messages\": [{\"role\": \"%s\", \"content.masked\": \"fnv1a:%016llx\"}]",
             role, mask_fnv1a(r->text));
    else {
      printf(", \"gen_ai.input.messages\": [{\"role\": \"%s\", \"content\": ", role);
      print_json_str(r->text);
      printf("}]");
    }
  }
  printf("}\n");
}
