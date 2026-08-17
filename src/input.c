#include "input.h"
#include "common.h"
#include "jsonlite.h"
#include <string.h>

/* claude-code native fields:
 *   ts / type(request|response|tool) / status / model / session / tool /
 *   text / tokens_in / tokens_out
 * codex native fields (alias table right column):
 *   timestamp / event_type(model_request|model_response|tool_use) /
 *   conversation_id / prompt|reply / input_tokens / output_tokens          */
void input_read_record(const char *line, record_t *r) {
  memset(r, 0, sizeof(*r));
  json_get_str(line, "ts", r->ts, sizeof(r->ts));
  json_get_str(line, "timestamp", r->ts, sizeof(r->ts));              /* codex */
  json_get_str(line, "type", r->type, sizeof(r->type));
  json_get_str(line, "event_type", r->type, sizeof(r->type));         /* codex */
  json_get_str(line, "status", r->status, sizeof(r->status));
  json_get_str(line, "model", r->model, sizeof(r->model));
  json_get_str(line, "session", r->session, sizeof(r->session));
  json_get_str(line, "conversation_id", r->session, sizeof(r->session)); /* codex */
  json_get_str(line, "tool", r->tool, sizeof(r->tool));
  json_get_str(line, "text", r->text, sizeof(r->text));
  json_get_str(line, "prompt", r->text, sizeof(r->text));             /* codex request */
  json_get_str(line, "reply", r->text, sizeof(r->text));              /* codex response */
  json_get_num(line, "tokens_in", &r->tin);
  json_get_num(line, "input_tokens", &r->tin);                        /* codex */
  json_get_num(line, "tokens_out", &r->tout);
  json_get_num(line, "output_tokens", &r->tout);                      /* codex */
  /* codex event semantics -> canonical */
  if (strcmp(r->type, "model_request") == 0)  strcpy(r->type, "request");
  if (strcmp(r->type, "model_response") == 0) strcpy(r->type, "response");
  if (strcmp(r->type, "tool_use") == 0)       strcpy(r->type, "tool");
}
