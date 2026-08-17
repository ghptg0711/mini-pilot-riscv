#include "input.h"
#include "common.h"
#include "jsonlite.h"
#include <string.h>

/* claude-code native fields:
 *   ts / type(request|response|tool) / status / model / session / tool /
 *   text / tokens_in / tokens_out
 * codex native fields:
 *   timestamp / event_type(model_request|model_response|tool_use) /
 *   conversation_id / prompt|reply / input_tokens / output_tokens
 * cursor native fields (simulated, simplified):
 *   time / kind(ai.request|ai.response|ai.tool) / conversationId /
 *   prompt|reply / tokensIn / tokensOut
 * qoder native fields (simulated, simplified):
 *   event(request|response|tool) / conv_id / content / in_tok / out_tok  */
void input_read_record(const char *line, record_t *r) {
  memset(r, 0, sizeof(*r));
  json_get_str(line, "ts", r->ts, sizeof(r->ts));
  json_get_str(line, "timestamp", r->ts, sizeof(r->ts));              /* codex */
  json_get_str(line, "time", r->ts, sizeof(r->ts));                   /* cursor */
  json_get_str(line, "type", r->type, sizeof(r->type));
  json_get_str(line, "event_type", r->type, sizeof(r->type));         /* codex */
  json_get_str(line, "kind", r->type, sizeof(r->type));               /* cursor */
  json_get_str(line, "event", r->type, sizeof(r->type));              /* qoder */
  json_get_str(line, "status", r->status, sizeof(r->status));
  json_get_str(line, "model", r->model, sizeof(r->model));
  json_get_str(line, "session", r->session, sizeof(r->session));
  json_get_str(line, "conversation_id", r->session, sizeof(r->session)); /* codex */
  json_get_str(line, "conversationId", r->session, sizeof(r->session));  /* cursor */
  json_get_str(line, "conv_id", r->session, sizeof(r->session));       /* qoder */
  json_get_str(line, "tool", r->tool, sizeof(r->tool));
  json_get_str(line, "text", r->text, sizeof(r->text));
  json_get_str(line, "prompt", r->text, sizeof(r->text));             /* codex/cursor request */
  json_get_str(line, "reply", r->text, sizeof(r->text));              /* codex/cursor response */
  json_get_str(line, "content", r->text, sizeof(r->text));            /* qoder */
  json_get_num(line, "tokens_in", &r->tin);
  json_get_num(line, "input_tokens", &r->tin);                        /* codex */
  json_get_num(line, "tokensIn", &r->tin);                            /* cursor */
  json_get_num(line, "in_tok", &r->tin);                              /* qoder */
  json_get_num(line, "tokens_out", &r->tout);
  json_get_num(line, "output_tokens", &r->tout);                      /* codex */
  json_get_num(line, "tokensOut", &r->tout);                          /* cursor */
  json_get_num(line, "out_tok", &r->tout);                            /* qoder */
  /* per-agent event semantics -> canonical */
  if (strcmp(r->type, "model_request") == 0)  strcpy(r->type, "request");
  if (strcmp(r->type, "model_response") == 0) strcpy(r->type, "response");
  if (strcmp(r->type, "tool_use") == 0)       strcpy(r->type, "tool");
  if (strcmp(r->type, "ai.request") == 0)     strcpy(r->type, "request");
  if (strcmp(r->type, "ai.response") == 0)    strcpy(r->type, "response");
  if (strcmp(r->type, "ai.tool") == 0)        strcpy(r->type, "tool");
}
